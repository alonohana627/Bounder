// The bosses: the Brute at the end of every fortress (walks at the hero and
// hops, faster in later worlds) and the King of the final castle (walks,
// leaps at the hero and throws arcing fireballs, more of everything as he
// weakens). Scripted with GameLib's StateMachine and nextPattern; stomps
// and fireballs hurt them, with a short stun after each hit.

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

constexpr int kBossHurtTicks = 50;

} // namespace

void Session::checkArena()
{
    if (m_arenaColumn < 0 || m_arenaClosed || m_area != 0) {
        return;
    }
    if (m_hero.body.box.left() < (static_cast<float>(m_arenaColumn) + 1.5f) * kTile) {
        return;
    }
    // The gate closes behind the hero.
    gamelib::core::TileLayer& layer = currentArea().map.layer(0);
    const int column = m_arenaColumn;
    for (int row = 0; row < layer.height(); ++row) {
        if (layer.inside({column, row}) && layer.at({column, row}) == ' ') {
            layer.at({column, row}) = 'X';
        }
    }
    m_arenaClosed = true;
    for (auto [handle, enemy] : currentArea().enemies) {
        if (enemy.kind == EnemyKind::Brute || enemy.kind == EnemyKind::King) {
            enemy.awake = true;
            enemy.boss.enter(BossState::Idle);
            enemy.dir = -1;
            m_events.push({.kind = EventKind::BossStart, .at = enemy.box.center(), .enemy = enemy.kind});
        }
    }
}

void Session::stepBoss(Enemy& e)
{
    const gamelib::core::TileWorld world = tileWorld();
    const Rect& hero = m_hero.body.box;
    e.hurtTicks = std::max(0, e.hurtTicks - 1);
    // Walk and fall with the boss's own velocity (GameLib's walker turns it
    // round at walls); true on the tick it lands.
    const auto fall = [&] {
        gamelib::core::WalkerBody body{.box = e.box, .velocity = e.velocity, .direction = e.dir, .grounded = e.grounded};
        const gamelib::core::WalkerStep step =
            gamelib::core::stepWalker(body, {.gravity = kEnemyGravity, .maxFall = kEnemyMaxFall + 1.0f}, world);
        e.box = body.box;
        e.velocity = body.velocity;
        e.dir = body.direction;
        e.grounded = body.grounded;
        return step.landed;
    };
    const int towards = gamelib::core::directionTowards(e.box.center().x, hero.center().x, e.dir);
    auto& sm = e.boss;

    if (e.kind == EnemyKind::Brute) {
        const float speed = 0.7f + 0.18f * static_cast<float>(worldOf(m_level)) + (e.hp <= 2 ? 0.3f : 0.0f);
        switch (sm.current()) {
        case BossState::Idle:
            e.velocity.x = 0.0f;
            fall();
            if (sm.after(50)) {
                sm.enter(BossState::Walk);
            }
            break;
        case BossState::Walk:
            if (sm.justEntered()) {
                e.dir = towards;
            }
            if (sm.ticksIn() % 40 == 0) {
                e.dir = towards; // turns towards the hero now and then, not at once
            }
            e.velocity.x = static_cast<float>(e.dir) * speed;
            fall();
            if (sm.after(100 - 10 * worldOf(m_level))) {
                sm.enter(BossState::Hop);
            }
            break;
        case BossState::Hop:
            if (sm.justEntered()) {
                e.velocity = {static_cast<float>(towards) * 1.4f, -5.0f};
                e.grounded = false;
            }
            if (fall() && sm.ticksIn() > 4) {
                m_events.push({.kind = EventKind::BossLand, .at = {e.box.center().x, e.box.bottom()}});
                sm.enter(BossState::Walk);
            }
            break;
        case BossState::Stunned:
            e.velocity.x = 0.0f;
            fall();
            if (sm.after(40)) {
                sm.enter(BossState::Hop);
            }
            break;
        case BossState::Leap:
        case BossState::Throw:
        case BossState::Defeated: fall(); break;
        }
        sm.tick();
        return;
    }

    // The King.
    const int phase = e.hp > 8 ? 0 : (e.hp > 4 ? 1 : 2);
    switch (sm.current()) {
    case BossState::Idle:
        e.velocity.x = 0.0f;
        fall();
        if (sm.after(phase == 0 ? 50 : 30)) {
            e.pattern = gamelib::core::nextPattern(m_rng, 3, e.pattern);
            sm.enter(e.pattern == 0 ? BossState::Walk : (e.pattern == 1 ? BossState::Leap : BossState::Throw));
        }
        break;
    case BossState::Walk:
        if (sm.justEntered() || sm.ticksIn() % 30 == 0) {
            e.dir = towards;
        }
        e.velocity.x = static_cast<float>(e.dir) * (0.8f + 0.3f * static_cast<float>(phase));
        fall();
        if (sm.after(90)) {
            sm.enter(BossState::Idle);
        }
        break;
    case BossState::Leap:
        if (sm.justEntered()) {
            e.velocity = {std::clamp((hero.center().x - e.box.center().x) / 55.0f, -2.6f, 2.6f), -6.0f};
            e.grounded = false;
        }
        if (fall() && sm.ticksIn() > 4) {
            m_events.push({.kind = EventKind::BossLand, .at = {e.box.center().x, e.box.bottom()}});
            sm.enter(BossState::Idle);
        }
        break;
    case BossState::Throw: {
        e.velocity.x = 0.0f;
        e.dir = towards;
        fall();
        const int shots = 3 + phase;
        const int t = sm.ticksIn();
        if (t >= 20 && (t - 20) % 16 == 0 && (t - 20) / 16 < shots) {
            // A fan of arcs around the hero's position.
            const float fan = static_cast<float>((t - 20) / 16) - static_cast<float>(shots - 1) / 2.0f;
            const Vec2 from{e.box.center().x + static_cast<float>(e.dir) * 14.0f, e.box.top() + 8.0f};
            const float dx = hero.center().x - from.x;
            Shots::Shot shot{.position = from,
                             .velocity = {std::clamp(dx * 0.85f, -240.0f, 240.0f) / 60.0f + fan * 0.67f, -3.85f},
                             .size = {9.0f, 9.0f},
                             .lifetimeTicks = 260,
                             .team = 1,
                             .owner = 1,
                             .damage = 1,
                             .tileContact = gamelib::core::TileContact::BounceOnFloor,
                             .gravity = 0.145f,
                             .bounceSpeed = 2.5f,
                             .maxBounces = 2,
                             .extra = {.boss = true}};
            if (m_shots.spawn(shot) != 0) {
                m_events.push({.kind = EventKind::Fireball, .at = from, .value = 1});
            }
        }
        if (sm.after(30 + 16 * shots)) {
            sm.enter(BossState::Idle);
        }
        break;
    }
    case BossState::Stunned:
        e.velocity.x = 0.0f;
        fall();
        if (sm.after(35)) {
            sm.enter(BossState::Leap);
        }
        break;
    case BossState::Hop:
    case BossState::Defeated: fall(); break;
    }
    sm.tick();
}

void Session::hitBoss(Enemy& e, const int damage, const Vec2 at)
{
    if (e.hurtTicks > 0 || e.boss.is(BossState::Defeated) || m_phase != Phase::Playing) {
        return;
    }
    e.hp = std::max(0, e.hp - damage);
    e.hurtTicks = kBossHurtTicks;
    m_events.push({.kind = EventKind::BossHit, .at = at, .value = e.hp, .enemy = e.kind});
    if (e.hp > 0) {
        e.boss.enter(BossState::Stunned);
        return;
    }
    e.boss.enter(BossState::Defeated);
    e.state = EnemyState::Knocked;
    e.velocity = {0.0f, -4.0f};
    m_shots.clear();
    for (auto [handle, other] : currentArea().enemies) {
        if (&other != &e && other.state != EnemyState::Knocked) {
            knockOut(other, 1);
        }
    }
    m_events.push({.kind = EventKind::BossDefeated, .at = e.box.center(), .enemy = e.kind});
    addPoints(5000, e.box.center());
    m_phase = Phase::BossDown;
    m_phaseTicks = 0;
}

} // namespace bounder::game
