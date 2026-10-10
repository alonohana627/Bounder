// The enemies of a level: walkers, shell walkers and their shells, hoppers,
// fliers, plants in pipes and spikies - each a small rule on GameLib's
// enemy movement helpers (stepWalker, everyTicks, sinePath) - and what
// happens when the hero meets them (stomp, kick, hurt).

#include "session.hpp"

#include <gamelib/core/collision.hpp>
#include <gamelib/core/enemy_motion.hpp>

#include <algorithm>
#include <cmath>

namespace bounder::game {

namespace {

// helpers

using gamelib::core::Rect;
using gamelib::core::Vec2;

constexpr int kPlantCycle = 240;
constexpr int kSquashedTicks = 30;
constexpr float kWakeDistance = 15.0f * kTile;

Rect _feetBox(const Vec2 feet, const Vec2 size)
{
    return {feet.x - size.x / 2.0f, feet.y - size.y, size.x, size.y};
}

bool _isBoss(const Enemy& e)
{
    return e.kind == EnemyKind::Brute || e.kind == EnemyKind::King;
}

bool _alive(const Enemy& e)
{
    return e.state != EnemyState::Knocked && e.state != EnemyState::Squashed;
}

// Walkers that turn when they bump into each other.
bool _walks(const Enemy& e)
{
    return e.state == EnemyState::Walking
           && (e.kind == EnemyKind::Walker || e.kind == EnemyKind::Shell || e.kind == EnemyKind::RedShell
               || e.kind == EnemyKind::Spiky || e.kind == EnemyKind::Hopper);
}

} // namespace

void Session::spawnEnemies(Area& area, const int areaIndex)
{
    (void)areaIndex;
    for (const gamelib::core::TileMarker& marker : area.map.markers()) {
        Enemy e;
        const Vec2 feet = area.map.cellBottom(marker.cell);
        const Vec2 center = area.map.cellCenter(marker.cell);
        switch (marker.symbol) {
        case 'g': e.kind = EnemyKind::Walker; e.box = _feetBox(feet, {14.0f, 14.0f}); break;
        case 'k': e.kind = EnemyKind::Shell; e.box = _feetBox(feet, {14.0f, 20.0f}); break;
        case 'r': e.kind = EnemyKind::RedShell; e.box = _feetBox(feet, {14.0f, 20.0f}); break;
        case 'j': e.kind = EnemyKind::Hopper; e.box = _feetBox(feet, {14.0f, 14.0f}); break;
        case 's': e.kind = EnemyKind::Spiky; e.box = _feetBox(feet, {14.0f, 14.0f}); break;
        case 'f': e.kind = EnemyKind::Flier; e.box = Rect::fromCenter(center, {14.0f, 12.0f}); break;
        case 'y': e.kind = EnemyKind::FlierVertical; e.box = Rect::fromCenter(center, {14.0f, 12.0f}); break;
        case 'p': {
            // The plant lives in the pipe right below its marker: hidden
            // inside it (its top at the pipe's top) until it rises.
            e.kind = EnemyKind::Plant;
            const float pipeTop = feet.y;
            e.box = {feet.x + kTile / 2.0f - 6.0f, pipeTop, 12.0f, 22.0f};
            break;
        }
        case 'Z': {
            const bool king = m_level == kCastleLevel;
            e.kind = king ? EnemyKind::King : EnemyKind::Brute;
            e.box = _feetBox(feet, king ? Vec2{34.0f, 40.0f} : Vec2{28.0f, 28.0f});
            e.hp = king ? 12 : 6;
            break;
        }
        default: continue;
        }
        e.origin = e.box.center();
        e.previous = e.box;
        e.vulnerable = gamelib::core::Vulnerable{e.box, 1};
        area.enemies.insert(e);
    }
}

void Session::stepEnemies()
{
    Area& area = currentArea();
    const float heroX = m_hero.body.box.center().x;
    for (auto [handle, enemy] : area.enemies) {
        if (!enemy.awake) {
            if (_isBoss(enemy) || std::abs(enemy.box.center().x - heroX) > kWakeDistance) {
                continue;
            }
            enemy.awake = true;
            enemy.dir = enemy.kind == EnemyKind::Flier || enemy.box.center().x > heroX ? -1 : 1;
        }
        ++enemy.age;
        ++enemy.timer;
        stepEnemy(enemy);
    }
    // Walkers that meet turn around (both, if they walk into each other).
    for (auto [ha, a] : area.enemies) {
        if (!a.awake || !_walks(a)) {
            continue;
        }
        for (auto [hb, b] : area.enemies) {
            if (hb <= ha || !b.awake || !_walks(b) || !gamelib::core::overlaps(a.box, b.box)) {
                continue;
            }
            const int towards = b.box.center().x > a.box.center().x ? 1 : -1;
            if (a.dir == towards) {
                a.dir = -a.dir;
            }
            if (b.dir == -towards) {
                b.dir = -b.dir;
            }
        }
    }
    const float bottom = area.map.bounds().bottom();
    area.enemies.eraseIf([&](gamelib::core::Handle<Enemy>, const Enemy& e) {
        return e.box.top() > bottom + 2.0f * kTile || (e.state == EnemyState::Squashed && e.timer >= kSquashedTicks);
    });
}

void Session::stepEnemy(Enemy& e)
{
    const gamelib::core::TileWorld world = tileWorld();
    if (e.state == EnemyState::Knocked) {
        e.box.x += e.velocity.x;
        e.box.y += e.velocity.y;
        e.velocity.y = std::min(e.velocity.y + kEnemyGravity, 6.0f);
        return;
    }
    if (e.state == EnemyState::Squashed) {
        return;
    }
    if (_isBoss(e)) {
        stepBoss(e);
        return;
    }
    // One tick of walking and falling through the tiles (GameLib's walker):
    // speed 0 keeps the enemy's own velocity.x (flipped at walls).
    const auto walk = [&](const float speed, const bool turnAtLedges = false) {
        gamelib::core::WalkerBody body{.box = e.box, .velocity = e.velocity, .direction = e.dir, .grounded = e.grounded};
        const gamelib::core::WalkerStep step = gamelib::core::stepWalker(
            body, {.gravity = kEnemyGravity, .maxFall = kEnemyMaxFall, .speed = speed, .turnAtLedges = turnAtLedges, .ledgeProbe = 2.0f},
            world);
        e.box = body.box;
        e.velocity = body.velocity;
        e.dir = body.direction;
        e.grounded = body.grounded;
        return step;
    };

    if (e.state == EnemyState::ShellIdle) {
        e.velocity.x = 0.0f;
        (void)walk(0.0f);
        if (e.timer >= kShellReviveTicks) {
            e.state = EnemyState::Walking;
            e.timer = 0;
            e.box = _feetBox({e.box.center().x, e.box.bottom()}, {14.0f, 20.0f});
            e.dir = e.box.center().x > m_hero.body.box.center().x ? -1 : 1;
        }
        return;
    }
    if (e.state == EnemyState::ShellSliding) {
        e.kickGrace = std::max(0, e.kickGrace - 1);
        if (walk(kShellSpeed).contacts.blockedX) {
            // A wall (the shell has already turned): knock a block in it like a head would.
            const float x = e.dir < 0 ? e.box.right() + 1.0f : e.box.left() - 1.0f;
            const gamelib::core::Cell cell = gamelib::core::cellAt({x, e.box.center().y}, kTile);
            const gamelib::core::TileLayer& layer = currentArea().map.layer(0);
            if (layer.inside(cell)) {
                const gamelib::core::TileId id = layer.at(cell);
                if (id == 'B' || id == '?' || id == 'P' || id == '*' || id == 'C') {
                    const Power power = m_hero.power;
                    m_hero.power = Power::Big; // a shell breaks bricks whatever the hero's size
                    hitBlock(cell);
                    m_hero.power = power;
                } else {
                    m_events.push({.kind = EventKind::BlockBump, .at = {x, e.box.center().y}, .value = 0});
                }
            }
        }
        shellHits(e);
        return;
    }

    switch (e.kind) {
    case EnemyKind::Walker:
    case EnemyKind::Shell:
    case EnemyKind::RedShell:
    case EnemyKind::Spiky: {
        // Red shells never walk off their platform.
        (void)walk(e.kind == EnemyKind::Spiky ? 0.45f : kWalkerSpeed, e.kind == EnemyKind::RedShell);
        break;
    }
    case EnemyKind::Hopper: {
        if (e.grounded) {
            e.velocity.x = 0.0f;
            const int offset = static_cast<int>(e.origin.x) % 37;
            if (gamelib::core::everyTicks(e.age, 80, offset)) {
                e.dir = gamelib::core::directionTowards(e.box.center().x, m_hero.body.box.center().x, e.dir);
                e.velocity = {static_cast<float>(e.dir) * 0.9f, -4.2f};
                e.grounded = false;
            }
        }
        (void)walk(0.0f);
        break;
    }
    case EnemyKind::Flier: {
        const Vec2 at = gamelib::core::sinePath(e.origin, {-0.4f, 0.0f}, {0.0f, 1.5f * kTile}, e.age, 120);
        e.box = Rect::fromCenter(at, e.box.size());
        e.dir = -1;
        break;
    }
    case EnemyKind::FlierVertical: {
        const Vec2 at = gamelib::core::sinePath(e.origin, {}, {0.0f, 2.5f * kTile}, e.age, 170);
        e.box = Rect::fromCenter(at, e.box.size());
        e.dir = gamelib::core::directionTowards(e.box.center().x, m_hero.body.box.center().x, e.dir);
        break;
    }
    case EnemyKind::Plant: {
        // Hidden, rising, out, sinking. It stays in while the hero stands
        // next to its pipe, so a pipe is never a trap.
        int t = e.timer % kPlantCycle;
        if (t == 89 && std::abs(m_hero.body.box.center().x - e.origin.x) < 2.0f * kTile) {
            --e.timer;
            t = 88;
        }
        const float pipeTop = e.origin.y - e.box.height / 2.0f; // the hidden box's top
        float out = 0.0f;
        if (t >= 90 && t < 120) {
            out = static_cast<float>(t - 90) / 30.0f;
        } else if (t >= 120 && t < 180) {
            out = 1.0f;
        } else if (t >= 180 && t < 210) {
            out = 1.0f - static_cast<float>(t - 180) / 30.0f;
        }
        e.box.y = pipeTop - out * e.box.height;
        break;
    }
    case EnemyKind::Brute:
    case EnemyKind::King: break;
    }
}

void Session::knockOut(Enemy& e, const int dir)
{
    e.state = EnemyState::Knocked;
    e.timer = 0;
    e.velocity = {static_cast<float>(dir) * 1.0f, -3.5f};
    m_events.push({.kind = EventKind::EnemyKnocked, .at = e.box.center(), .enemy = e.kind});
}

void Session::shellHits(Enemy& shell)
{
    for (auto [handle, other] : currentArea().enemies) {
        if (&other == &shell || !other.awake || !_alive(other) || _isBoss(other)) {
            continue;
        }
        if (other.kind == EnemyKind::Plant && other.box.bottom() >= other.origin.y + other.box.height / 2.0f - 1.0f) {
            continue; // hidden in its pipe
        }
        if (!gamelib::core::overlaps(shell.box, other.box)) {
            continue;
        }
        if (other.state == EnemyState::ShellSliding) {
            knockOut(shell, -shell.dir);
        }
        knockOut(other, shell.dir);
        addChainPoints(shell.chain, other.box.center());
        if (shell.state == EnemyState::Knocked) {
            return;
        }
    }
}

void Session::heroMeetsEnemies(const float heroFall)
{
    Hero& h = m_hero;
    bool stomped = false;
    for (auto [handle, e] : currentArea().enemies) {
        if (m_phase != Phase::Playing) {
            return;
        }
        if (!e.awake || !_alive(e) || !gamelib::core::overlaps(h.body.box, e.box)) {
            continue;
        }
        if (_isBoss(e)) {
            if (e.boss.is(BossState::Defeated)) {
                continue;
            }
            if (!stomped && gamelib::core::stompsFromAbove(h.body.box, heroFall, e.box, 10.0f)) {
                stomped = true;
                gamelib::core::launch(h.body, {h.body.velocity.x, -kStompBounce * 1.1f}, true);
                h.body.box.y = e.box.top() - h.body.box.height;
                if (e.hurtTicks == 0) {
                    hitBoss(e, e.kind == EnemyKind::King ? 3 : 2, {h.body.box.center().x, e.box.top()});
                }
            } else if (e.hurtTicks == 0) {
                hurtHero();
            }
            continue;
        }
        if (h.starTicks > 0) {
            knockOut(e, h.body.box.center().x < e.box.center().x ? 1 : -1);
            addChainPoints(h.chain, e.box.center());
            continue;
        }
        if (e.kind == EnemyKind::Plant && e.box.bottom() >= e.origin.y + e.box.height / 2.0f - 1.0f) {
            continue; // fully inside its pipe
        }
        const bool stompable = e.kind != EnemyKind::Plant && e.kind != EnemyKind::Spiky;
        if (stompable && !stomped && gamelib::core::stompsFromAbove(h.body.box, heroFall, e.box, 6.0f)) {
            stomped = true;
            gamelib::core::launch(h.body, {h.body.velocity.x, -kStompBounce}, true);
            h.body.box.y = std::min(h.body.box.y, e.box.top() - h.body.box.height);
            const Vec2 at{e.box.center().x, e.box.top()};
            if (e.state == EnemyState::ShellIdle) {
                e.state = EnemyState::ShellSliding;
                e.dir = h.body.box.center().x < e.box.center().x ? 1 : -1;
                e.kickGrace = 12;
                e.chain = 0;
                e.timer = 0;
                m_events.push({.kind = EventKind::Kick, .at = at});
                addPoints(400, at);
                continue;
            }
            m_events.push({.kind = EventKind::Stomp, .at = at, .value = h.chain, .enemy = e.kind});
            addChainPoints(h.chain, at);
            if (e.state == EnemyState::ShellSliding) {
                e.state = EnemyState::ShellIdle;
                e.timer = 0;
            } else if (e.kind == EnemyKind::Shell || e.kind == EnemyKind::RedShell) {
                e.state = EnemyState::ShellIdle;
                e.timer = 0;
                e.velocity = {};
                e.box = _feetBox({e.box.center().x, e.box.bottom()}, {14.0f, 14.0f});
            } else {
                e.state = EnemyState::Squashed;
                e.timer = 0;
                e.box = _feetBox({e.box.center().x, e.box.bottom()}, {14.0f, 6.0f});
            }
            continue;
        }
        if (e.state == EnemyState::ShellIdle) {
            // A side touch kicks an idle shell away.
            e.state = EnemyState::ShellSliding;
            e.dir = h.body.box.center().x < e.box.center().x ? 1 : -1;
            e.kickGrace = 12;
            e.chain = 0;
            e.timer = 0;
            e.box.x += static_cast<float>(e.dir) * 3.0f;
            m_events.push({.kind = EventKind::Kick, .at = e.box.center()});
            addPoints(400, e.box.center());
            continue;
        }
        if (e.state == EnemyState::ShellSliding && e.kickGrace > 0) {
            continue;
        }
        hurtHero();
    }
}

} // namespace bounder::game
