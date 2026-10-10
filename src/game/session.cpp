#include "session.hpp"

#include <gamelib/core/collision.hpp>
#include <gamelib/core/replay.hpp>
#include <gamelib/core/world_graph.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace bounder::game {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Rect;
using gamelib::core::TileShape;
using gamelib::core::Vec2;

constexpr float kTickSeconds = 1.0f / static_cast<float>(kTickRate);
constexpr int kBumpTicks = 12;
constexpr int kCoinBrickTicks = 240; // a multi-coin brick runs dry this long after its first coin
constexpr float kPoleHeight = 9.5f * kTile;
constexpr int kGoalWalkTicks = 110;
constexpr int kBossDownTicks = 150;

const gamelib::core::TileShapes& _shapes()
{
    static const gamelib::core::TileShapes shapes{
        {'#', TileShape::Solid},  {'X', TileShape::Solid},       {'B', TileShape::Solid},
        {'C', TileShape::Solid},  {'?', TileShape::Solid},       {'P', TileShape::Solid},
        {'*', TileShape::Solid},  {'u', TileShape::Solid},       {'{', TileShape::Solid},
        {'}', TileShape::Solid},  {'[', TileShape::Solid},       {']', TileShape::Solid},
        {'E', TileShape::Solid},  {'O', TileShape::Solid},
        {'=', TileShape::OneWay}, {'/', TileShape::RampUpRight}, {'\\', TileShape::RampUpLeft},
        {'a', TileShape::RampUpRightLow}, {'A', TileShape::RampUpRightHigh},
        {'D', TileShape::RampUpLeftHigh}, {'d', TileShape::RampUpLeftLow},
        {'^', TileShape::Hazard}, {'~', TileShape::Hazard},
    };
    return shapes;
}

Rect _boxAt(const Vec2 feet, const Vec2 size)
{
    return {feet.x - size.x / 2.0f, feet.y - size.y, size.x, size.y};
}

// Fireballs and boss shots: solid tiles, slopes as their whole cell, and
// one-way platforms from above only (a shot's box just entering the top).
bool _shotSolid(const gamelib::core::TileWorld& world, const Rect& box)
{
    const gamelib::core::CellRange cells = gamelib::core::cellsOverlapping(box, kTile);
    for (int y = cells.min.y; y <= cells.max.y; ++y) {
        for (int x = cells.min.x; x <= cells.max.x; ++x) {
            const TileShape shape = world.shapeAt({x, y});
            if (shape == TileShape::Solid || gamelib::core::isRamp(shape)) {
                return true;
            }
            if (shape == TileShape::OneWay && box.bottom() - static_cast<float>(y) * kTile < 4.0f) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

Session::Session(const int level, const Carry& carry)
    : m_level{level}
    , m_start{carry}
    , m_run{carry.run}
    , m_rng{0xB0D0'0000ULL + static_cast<std::uint64_t>(level)}
    , m_levelPoints{carry.levelPoints}
{
    if (level < 0 || level >= kLevelCount) {
        throw std::out_of_range{"Bounder: no such level"};
    }
    m_time = def().time;
    loadAreas();
    for (StarCoin& coin : m_starCoins) {
        coin.taken = (carry.starCoins & (1u << coin.index)) != 0;
    }
    const Area& main = m_areas.front();
    const std::vector<gamelib::core::TileMarker> starts = main.map.markers("S");
    if (starts.empty()) {
        throw std::logic_error{"Bounder: a level without a start"};
    }
    Vec2 feet = main.map.cellBottom(starts.front().cell);
    if (carry.checkpoint && main.checkpoint.x >= 0) {
        m_checkpointReached = true;
        feet = main.map.cellBottom(main.checkpoint) + Vec2{6.0f, 0.0f};
    }
    m_hero.power = m_run.power;
    spawnHero(feet);
}

gamelib::core::TileWorld Session::tileWorld() const
{
    return gamelib::core::TileWorld{map(), 0, _shapes()};
}

void Session::loadAreas()
{
    const LevelDef& d = def();
    const std::array<gamelib::core::LayerMapping, 1> layers{gamelib::core::charTile};
    m_areas.clear();
    m_starCoins.clear();
    for (std::size_t a = 0; a < d.areas.size(); ++a) {
        Area& area = m_areas.emplace_back();
        area.theme = d.areas[a].theme;
        const std::vector<std::string_view> rows{d.areas[a].rows.begin(), d.areas[a].rows.end()};
        area.map = gamelib::core::TileMap::fromText(rows, kTile, layers, kMarkerSymbols);
        const int areaIndex = static_cast<int>(a);
        for (const gamelib::core::TileMarker& marker : area.map.markers()) {
            switch (marker.symbol) {
            case 'F': area.checkpoint = marker.cell; break;
            case 'G': area.goal = marker.cell; break;
            case '1':
            case '2':
            case '3':
                m_starCoins.push_back({.index = marker.symbol - '1', .area = areaIndex,
                                       .center = area.map.cellBottom(marker.cell) + Vec2{0.0f, -kTile * 0.75f}});
                break;
            case 'w': {
                const Rect box{static_cast<float>(marker.cell.x) * kTile, static_cast<float>(marker.cell.y) * kTile,
                               3.0f * kTile, kTile * 0.5f};
                area.platforms.push_back({.box = box, .previous = box, .id = static_cast<int>(area.platforms.size()),
                                          .falling = true});
                break;
            }
            case 'm': {
                // The rail through the marker: '-' to either side, or '|' above and below.
                const gamelib::core::TileLayer& rails = area.map.layer(0);
                const auto railAt = [&](const Cell c, const gamelib::core::TileId id) { return rails.inside(c) && rails.at(c) == id; };
                Cell low = marker.cell;
                Cell high = marker.cell;
                while (railAt({low.x - 1, low.y}, '-')) --low.x;
                while (railAt({high.x + 1, high.y}, '-')) ++high.x;
                if (low == high) {
                    while (railAt({low.x, low.y - 1}, '|')) --low.y;
                    while (railAt({high.x, high.y + 1}, '|')) ++high.y;
                } else {
                    high.x = std::max(low.x, high.x - 2); // the left end travels; the platform is 3 wide
                }
                const Vec2 from{static_cast<float>(low.x) * kTile, static_cast<float>(low.y) * kTile};
                const Vec2 to{static_cast<float>(high.x) * kTile, static_cast<float>(high.y) * kTile};
                const Vec2 start{static_cast<float>(marker.cell.x) * kTile, static_cast<float>(marker.cell.y) * kTile};
                const Rect box{start.x, start.y, 3.0f * kTile, kTile * 0.5f};
                area.platforms.push_back({.path = {.points = {from, to}, .end = gamelib::core::Loop::PingPong},
                                          .startOffset = gamelib::core::length(start - from),
                                          .box = box, .previous = box, .id = static_cast<int>(area.platforms.size())});
                break;
            }
            case 'z': m_arenaColumn = a == 0 ? marker.cell.x : m_arenaColumn; break;
            default: break;
            }
        }
        const gamelib::core::TileLayer& layer = area.map.layer(0);
        for (int y = 0; y < layer.height(); ++y) {
            for (int x = 0; x < layer.width(); ++x) {
                if (layer.at({x, y}) == 'C') {
                    area.coinBricks.push_back({.cell = {x, y}});
                }
            }
        }
        spawnEnemies(area, areaIndex);
    }
    std::ranges::sort(m_starCoins, {}, &StarCoin::index);
    linkPipes();
}

void Session::linkPipes()
{
    // Pipe tops by kind, left to right: the level's k-th E leads to bonus
    // room k + 1's O; that room's E leads back to the level's k-th O.
    const auto find = [](const Area& area, const gamelib::core::TileId id) {
        std::vector<Cell> cells;
        const gamelib::core::TileLayer& layer = area.map.layer(0);
        for (int x = 0; x < layer.width(); ++x) {
            for (int y = 0; y < layer.height(); ++y) {
                if (layer.at({x, y}) == id) {
                    cells.push_back({x, y});
                }
            }
        }
        return cells;
    };
    const std::vector<Cell> entrances = find(m_areas.front(), 'E');
    const std::vector<Cell> returns = find(m_areas.front(), 'O');
    for (std::size_t k = 0; k < entrances.size() && k + 1 < m_areas.size(); ++k) {
        Area& room = m_areas[k + 1];
        const std::vector<Cell> arrive = find(room, 'O');
        const std::vector<Cell> leave = find(room, 'E');
        if (arrive.empty()) {
            throw std::logic_error{"Bounder: a bonus room without an arrival pipe"};
        }
        m_areas.front().pipes.push_back({.top = entrances[k], .targetArea = static_cast<int>(k + 1), .targetTop = arrive.front()});
        if (!leave.empty() && k < returns.size()) {
            room.pipes.push_back({.top = leave.front(), .targetArea = 0, .targetTop = returns[k]});
        }
    }
}

void Session::debugStartAt(const int column)
{
    const gamelib::core::TileWorld world = tileWorld();
    const int x = std::clamp(column, 0, map().width() - 1);
    for (int y = 1; y < map().height(); ++y) {
        if (world.shapeAt({x, y}) == TileShape::Solid && world.shapeAt({x, y - 1}) == TileShape::Empty) {
            spawnHero(map().cellBottom({x, y - 1}));
            return;
        }
    }
}

void Session::spawnHero(const Vec2 feet)
{
    m_hero.body = {};
    m_hero.body.box = _boxAt(feet, heroSize(m_hero.power));
    m_hero.body.grounded = true;
    m_hero.previous = m_hero.body.box;
    m_hero.vulnerable = gamelib::core::Vulnerable{m_hero.body.box, 0};
}

void Session::setPower(const Power power)
{
    const Rect old = m_hero.body.box;
    m_hero.power = power;
    m_run.power = power;
    m_hero.body.box = _boxAt({old.center().x, old.bottom()}, heroSize(power));
    m_hero.previous = m_hero.body.box;
}

Carry Session::carry() const
{
    Carry next{.run = m_run, .checkpoint = false, .starCoins = starCoinMask(), .levelPoints = m_levelPoints};
    if (m_outcome == Outcome::Died) {
        next.run.power = Power::Small;
        next.checkpoint = m_checkpointReached;
    } else {
        next.starCoins = 0;
        next.levelPoints = 0;
    }
    return next;
}

std::uint8_t Session::starCoinMask() const
{
    std::uint8_t mask = 0;
    for (const StarCoin& coin : m_starCoins) {
        if (coin.taken) {
            mask = static_cast<std::uint8_t>(mask | (1u << coin.index));
        }
    }
    return mask;
}

std::optional<Rect> Session::lockedArena() const
{
    if (!m_arenaClosed || m_arenaColumn < 0 || m_area != 0) {
        return std::nullopt;
    }
    const Rect bounds = m_areas.front().map.bounds();
    const float left = static_cast<float>(m_arenaColumn) * kTile;
    return Rect{left, bounds.y, bounds.right() - left, bounds.height};
}

const Enemy* Session::boss() const
{
    if (m_areas.empty()) {
        return nullptr;
    }
    for (const auto [handle, enemy] : m_areas.front().enemies) {
        if (enemy.kind == EnemyKind::Brute || enemy.kind == EnemyKind::King) {
            return &enemy;
        }
    }
    return nullptr;
}

void Session::tick(const Input& input)
{
    // Every object's previous box first, so frozen phases draw still.
    m_hero.previous = m_hero.body.box;
    Area& area = currentArea();
    for (auto [handle, enemy] : area.enemies) {
        enemy.previous = enemy.box;
    }
    for (Item& item : area.items) {
        item.previous = item.box;
    }
    for (Platform& platform : area.platforms) {
        platform.previous = platform.box;
    }
    for (Bump& bump : area.bumps) {
        ++bump.ticks;
    }
    std::erase_if(area.bumps, [](const Bump& b) { return b.ticks >= kBumpTicks; });

    ++m_phaseTicks;
    switch (m_phase) {
    case Phase::Playing: playTick(input); break;
    case Phase::Growing:
        if (m_phaseTicks >= kGrowTicks) {
            m_phase = Phase::Playing;
            m_phaseTicks = 0;
        }
        break;
    case Phase::PipeIn: {
        Rect& box = m_hero.body.box;
        box.y += (box.height + 2.0f) / static_cast<float>(kPipeTicks);
        if (m_phaseTicks >= kPipeTicks && m_pipe) {
            m_area = m_pipe->targetArea;
            m_shots.clear();
            const Vec2 top = currentArea().map.cellBottom(m_pipe->targetTop) + Vec2{kTile / 2.0f, -kTile};
            m_pipeCenter = top;
            box.x = top.x - box.width / 2.0f;
            box.y = top.y + 2.0f;
            m_hero.previous = box;
            m_phase = Phase::PipeOut;
            m_phaseTicks = 0;
            m_events.push({.kind = EventKind::PipeOut, .at = top});
        }
        break;
    }
    case Phase::PipeOut: {
        Rect& box = m_hero.body.box;
        box.y -= (box.height + 2.0f) / static_cast<float>(kPipeTicks);
        if (m_phaseTicks >= kPipeTicks) {
            box.y = m_pipeCenter.y - box.height;
            m_hero.body.velocity = {};
            m_hero.body.grounded = true;
            m_pipe.reset();
            m_phase = Phase::Playing;
            m_phaseTicks = 0;
        }
        break;
    }
    case Phase::Dying: dyingTick(); break;
    case Phase::Goal: goalTick(input); break;
    case Phase::BossDown:
        stepEnemies(); // the beaten boss falls away
        if (m_phaseTicks >= kBossDownTicks) {
            addPoints(m_time * kTimeBonusPerUnit, m_hero.body.box.center() + Vec2{0.0f, -20.0f});
            m_time = 0;
            m_events.push({.kind = EventKind::Clear, .at = m_hero.body.box.center()});
            m_outcome = Outcome::Cleared;
            m_phase = Phase::Over;
            m_phaseTicks = 0;
        }
        break;
    case Phase::Over: break;
    }
}

void Session::playTick(const Input& input)
{
    ++m_ticks;
    stepTimer();
    if (m_phase != Phase::Playing) {
        return;
    }
    stepPlatforms();
    const float bottomBefore = m_hero.body.box.bottom();
    stepHero(input);
    if (m_phase != Phase::Playing) {
        return;
    }
    const float heroFall = m_hero.body.box.bottom() - bottomBefore;
    stepItems();
    if (m_phase != Phase::Playing) {
        return;
    }
    stepEnemies();
    heroMeetsEnemies(heroFall);
    if (m_phase != Phase::Playing) {
        return;
    }
    stepShots();
    checkArena();
    m_hero.vulnerable.tick();
    m_hero.starTicks = std::max(0, m_hero.starTicks - 1);
    m_hero.throwTicks = std::max(0, m_hero.throwTicks - 1);
    if (m_phase == Phase::Playing && gamelib::core::exitSide(m_hero.body.box, map().bounds()) == gamelib::core::Direction::Down) {
        die(); // a pit
    }
}

void Session::stepTimer()
{
    if (++m_timeTicks < kTimeUnitTicks) {
        return;
    }
    m_timeTicks = 0;
    m_time = std::max(0, m_time - 1);
    if (m_time == kHurryUpTime && !m_hurried) {
        m_hurried = true;
        m_events.push({.kind = EventKind::HurryUp, .at = m_hero.body.box.center()});
    }
    if (m_time == 0) {
        die();
    }
}

void Session::stepPlatforms()
{
    Area& area = currentArea();
    for (Platform& p : area.platforms) {
        p.delta = {};
        if (p.falling) {
            if (m_hero.body.ridingId == p.id && m_hero.body.grounded) {
                if (++p.standTicks == 14) {
                    p.dropping = true;
                    m_events.push({.kind = EventKind::PlatformDrop, .at = p.box.center()});
                }
            }
            if (p.dropping) {
                p.fallSpeed = std::min(p.fallSpeed + 0.08f, 3.0f);
                p.delta = {0.0f, p.fallSpeed};
            }
        } else if (!p.path.points.empty()) {
            const double distance = static_cast<double>(p.startOffset)
                                    + static_cast<double>(kPlatformSpeed) * static_cast<double>(m_ticks);
            p.delta = p.path.pointAt(distance) - p.box.position();
        }
        p.box.x += p.delta.x;
        p.box.y += p.delta.y;
    }
    std::erase_if(area.platforms, [&](const Platform& p) { return p.box.top() > area.map.bounds().bottom() + 2.0f * kTile; });
}

void Session::stepHero(const Input& input)
{
    Hero& h = m_hero;
    gamelib::core::PlatformerBody& body = h.body;
    const gamelib::core::TileWorld world = tileWorld();
    const Area& area = currentArea();

    // Running down a slope is faster: the foot stands on a ramp falling the way the hero runs.
    bool downhill = false;
    if (body.grounded && input.move != 0) {
        const Vec2 foot{body.box.center().x, body.box.bottom() - 1.0f};
        for (const float dy : {0.0f, kTile}) {
            const TileShape shape = world.shapeAt(gamelib::core::cellAt(foot + Vec2{0.0f, dy}, kTile));
            const bool risesRight = shape == TileShape::RampUpRight || shape == TileShape::RampUpRightLow
                                    || shape == TileShape::RampUpRightHigh;
            const bool risesLeft = shape == TileShape::RampUpLeft || shape == TileShape::RampUpLeftLow
                                   || shape == TileShape::RampUpLeftHigh;
            if ((risesRight && input.move < 0) || (risesLeft && input.move > 0)) {
                downhill = true;
            }
        }
    }
    h.running = input.runHeld;
    const gamelib::core::PlatformerTuning tuning = tuningFor(input.runHeld, body.velocity.x, downhill, area.theme == Theme::Ice);

    std::vector<gamelib::core::MovingPlatform> platforms;
    for (const Platform& p : area.platforms) {
        platforms.push_back({.box = p.box, .delta = p.delta, .id = p.id});
    }
    const gamelib::core::PlatformerIntent intent{
        .move = static_cast<float>(input.move),
        .climb = 0.0f,
        .jumpPressed = input.jumpPressed,
        .jumpHeld = input.jumpHeld || input.jumpPressed,
    };
    const float topBefore = body.box.top();
    const float vyBefore = body.velocity.y;
    const float xBefore = body.box.x;
    const bool wasSkidding = h.skidding;
    const gamelib::core::PlatformerStep step = gamelib::core::stepPlatformer(body, intent, tuning, world, platforms);

    // The level's sides are walls.
    const float right = area.map.bounds().right() - body.box.width;
    if (body.box.x < 0.0f || body.box.x > right) {
        body.box.x = std::clamp(body.box.x, 0.0f, right);
        body.velocity.x = 0.0f;
    }
    if (body.grounded) {
        h.walked += std::abs(body.box.x - xBefore);
    }
    h.skidding = body.grounded && input.move != 0 && body.velocity.x * static_cast<float>(input.move) < 0.0f
                 && std::abs(body.velocity.x) > 0.9f;
    if (h.skidding && !wasSkidding) {
        m_events.push({.kind = EventKind::Skid, .at = {body.box.center().x, body.box.bottom()}});
    }
    const Vec2 feet{body.box.center().x, body.box.bottom()};
    if (step.events.jumped) {
        m_events.push({.kind = EventKind::Jump, .at = feet});
    }
    if (step.events.landed) {
        m_events.push({.kind = EventKind::Land, .at = feet});
    }
    if (body.grounded) {
        h.chain = 0;
    }
    if (step.events.hitHead || vyBefore < 0.0f) {
        hitBlockAbove(topBefore, vyBefore, step.events.hitHead);
    }
    collectCoins();
    collectStarCoins();
    if (step.contacts.hazard) {
        const gamelib::core::TileId id = area.map.layer(0).at(step.contacts.hazardCell);
        if (id == '~') {
            die(); // lava
            return;
        }
        hurtHero();
        if (m_phase != Phase::Playing) {
            return;
        }
    }
    checkPipes(input);
    if (m_phase != Phase::Playing) {
        return;
    }
    checkCheckpointAndGoal();
    if (m_phase != Phase::Playing) {
        return;
    }
    if (input.runPressed && h.power == Power::Fire) {
        throwFireball();
    }
    h.vulnerable.setHurtbox(body.box.expanded(-1.0f));
}

void Session::hitBlockAbove(const float topBefore, const float vyBefore, const bool hitHead)
{
    gamelib::core::PlatformerBody& body = m_hero.body;
    Area& area = currentArea();
    gamelib::core::TileLayer& layer = area.map.layer(0);
    const float centerX = body.box.center().x;
    const int column = static_cast<int>(std::floor(centerX / kTile));

    // A hidden block reveals itself when the head passes into it from below.
    if (vyBefore < 0.0f && topBefore > body.box.top()) {
        const int firstRow = static_cast<int>(std::floor(body.box.top() / kTile));
        const int lastRow = static_cast<int>(std::floor((topBefore - 0.01f) / kTile));
        for (int row = lastRow; row >= firstRow; --row) {
            const Cell cell{column, row};
            if (!layer.inside(cell)) {
                continue;
            }
            const gamelib::core::TileId id = layer.at(cell);
            if ((id == 'h' || id == 'H') && static_cast<float>(row + 1) * kTile <= topBefore + 0.01f) {
                layer.at(cell) = 'u';
                body.box.y = static_cast<float>(row + 1) * kTile;
                body.velocity.y = 0.0f;
                body.rising = false;
                area.bumps.push_back({.cell = cell});
                const Vec2 top{(static_cast<float>(column) + 0.5f) * kTile, static_cast<float>(row) * kTile};
                if (id == 'h') {
                    addCoin(top);
                    m_events.push({.kind = EventKind::Coin, .at = top, .value = 1});
                    addPoints(200, top);
                } else {
                    const Rect box{top.x - 6.0f, top.y + 2.0f, 12.0f, 12.0f};
                    area.items.push_back({.kind = ItemKind::OneUp, .box = box, .previous = box, .rise = kItemRiseTicks});
                    m_events.push({.kind = EventKind::ItemSpawn, .at = top, .item = ItemKind::OneUp});
                }
                bumpAbove(cell);
                return;
            }
        }
    }
    if (!hitHead) {
        return;
    }
    // The block right above the head: the column under the center first.
    const int row = static_cast<int>(std::floor((body.box.top() - 0.5f) / kTile));
    const int left = static_cast<int>(std::floor((body.box.left() + 0.01f) / kTile));
    const int right = static_cast<int>(std::floor((body.box.right() - 0.01f) / kTile));
    const auto isBlock = [&](const Cell c) {
        if (!layer.inside(c)) {
            return false;
        }
        const gamelib::core::TileId id = layer.at(c);
        return id == '?' || id == 'P' || id == '*' || id == 'B' || id == 'C';
    };
    Cell target{column, row};
    if (!isBlock(target)) {
        const Cell other{column == left ? right : left, row};
        if (isBlock(other)) {
            target = other;
        }
    }
    hitBlock(target);
}

void Session::hitBlock(const Cell cell)
{
    Area& area = currentArea();
    gamelib::core::TileLayer& layer = area.map.layer(0);
    if (!layer.inside(cell)) {
        return;
    }
    const gamelib::core::TileId id = layer.at(cell);
    const Vec2 top{(static_cast<float>(cell.x) + 0.5f) * kTile, static_cast<float>(cell.y) * kTile};
    const auto spawnItem = [&](const ItemKind kind) {
        const Rect box{top.x - 7.0f, top.y + 2.0f, 14.0f, 14.0f};
        area.items.push_back({.kind = kind, .box = box, .previous = box, .rise = kItemRiseTicks,
                              .dir = m_hero.body.box.center().x < top.x ? 1 : -1});
        m_events.push({.kind = EventKind::ItemSpawn, .at = top, .item = kind});
    };
    switch (id) {
    case '?':
        layer.at(cell) = 'u';
        area.bumps.push_back({.cell = cell});
        addCoin(top);
        m_events.push({.kind = EventKind::Coin, .at = top, .value = 1});
        addPoints(200, top);
        bumpAbove(cell);
        break;
    case 'P':
        layer.at(cell) = 'u';
        area.bumps.push_back({.cell = cell});
        spawnItem(m_hero.power == Power::Small ? ItemKind::Mushroom : ItemKind::Flower);
        bumpAbove(cell);
        break;
    case '*':
        layer.at(cell) = 'u';
        area.bumps.push_back({.cell = cell});
        spawnItem(ItemKind::Star);
        bumpAbove(cell);
        break;
    case 'C': {
        auto it = std::ranges::find(area.coinBricks, cell, &CoinBrick::cell);
        if (it == area.coinBricks.end()) {
            break;
        }
        it->started = true;
        --it->coinsLeft;
        area.bumps.push_back({.cell = cell});
        addCoin(top);
        m_events.push({.kind = EventKind::Coin, .at = top, .value = 1});
        addPoints(200, top);
        if (it->coinsLeft <= 0 || it->timer >= kCoinBrickTicks) {
            layer.at(cell) = 'u';
            area.coinBricks.erase(it);
        }
        bumpAbove(cell);
        break;
    }
    case 'B':
        if (m_hero.power != Power::Small) {
            layer.at(cell) = ' ';
            m_events.push({.kind = EventKind::BrickBreak, .at = top + Vec2{0.0f, kTile / 2.0f}});
            m_run.score += kBrickPoints;
            m_levelPoints += kBrickPoints;
        } else {
            area.bumps.push_back({.cell = cell});
            m_events.push({.kind = EventKind::BlockBump, .at = top, .value = 1});
        }
        bumpAbove(cell);
        break;
    default: m_events.push({.kind = EventKind::BlockBump, .at = top, .value = 0}); break;
    }
}

void Session::bumpAbove(const Cell cell)
{
    Area& area = currentArea();
    const Rect above{static_cast<float>(cell.x) * kTile, static_cast<float>(cell.y) * kTile - 3.0f, kTile, 3.0f};
    for (auto [handle, enemy] : area.enemies) {
        if (enemy.state == EnemyState::Knocked || enemy.state == EnemyState::Squashed || enemy.kind == EnemyKind::Brute
            || enemy.kind == EnemyKind::King || enemy.kind == EnemyKind::Plant) {
            continue;
        }
        if (gamelib::core::overlaps(above, enemy.box)) {
            knockOut(enemy, enemy.box.center().x < above.center().x ? -1 : 1);
            addPoints(kFireballKillPoints, enemy.box.center());
        }
    }
    for (Item& item : area.items) {
        if (item.rise == 0 && gamelib::core::overlaps(above, item.box)) {
            item.velocity.y = -3.0f;
            item.dir = item.box.center().x < above.center().x ? -1 : 1;
        }
    }
    gamelib::core::TileLayer& layer = area.map.layer(0);
    const Cell up{cell.x, cell.y - 1};
    if (layer.inside(up) && layer.at(up) == 'o') {
        layer.at(up) = ' ';
        const Vec2 at = area.map.cellCenter(up);
        addCoin(at);
        m_events.push({.kind = EventKind::Coin, .at = at, .value = 1});
    }
}

void Session::collectCoins()
{
    Area& area = currentArea();
    gamelib::core::TileLayer& layer = area.map.layer(0);
    const gamelib::core::CellRange cells = gamelib::core::cellsOverlapping(m_hero.body.box.expanded(-1.0f), kTile);
    for (int y = cells.min.y; y <= cells.max.y; ++y) {
        for (int x = cells.min.x; x <= cells.max.x; ++x) {
            if (layer.inside({x, y}) && layer.at({x, y}) == 'o') {
                layer.at({x, y}) = ' ';
                addCoin(area.map.cellCenter({x, y}));
                m_events.push({.kind = EventKind::Coin, .at = area.map.cellCenter({x, y}), .value = 0});
            }
        }
    }
    for (CoinBrick& brick : area.coinBricks) {
        if (brick.started) {
            ++brick.timer;
        }
    }
}

void Session::collectStarCoins()
{
    for (StarCoin& coin : m_starCoins) {
        if (coin.taken || coin.area != m_area) {
            continue;
        }
        if (gamelib::core::overlaps(coin.center, kTile * 0.8f, m_hero.body.box)) {
            coin.taken = true;
            m_events.push({.kind = EventKind::StarCoin, .at = coin.center, .value = coin.index});
            addPoints(kStarCoinPoints, coin.center);
        }
    }
}

void Session::checkPipes(const Input& input)
{
    if (!input.down || !m_hero.body.grounded) {
        return;
    }
    const Rect& box = m_hero.body.box;
    for (const PipeLink& link : currentArea().pipes) {
        const float left = static_cast<float>(link.top.x) * kTile;
        const float top = static_cast<float>(link.top.y) * kTile;
        if (std::abs(box.bottom() - top) < 0.5f && box.center().x >= left + 5.0f && box.center().x <= left + 2.0f * kTile - 5.0f) {
            enterPipe(link);
            return;
        }
    }
}

void Session::enterPipe(const PipeLink& link)
{
    m_pipe = link;
    m_phase = Phase::PipeIn;
    m_phaseTicks = 0;
    const float left = static_cast<float>(link.top.x) * kTile;
    m_pipeCenter = {left + kTile, static_cast<float>(link.top.y) * kTile};
    m_hero.body.box.x = m_pipeCenter.x - m_hero.body.box.width / 2.0f;
    m_hero.previous = m_hero.body.box;
    m_hero.body.velocity = {};
    m_hero.body.ridingId = -1;
    m_shots.clear();
    m_events.push({.kind = EventKind::PipeIn, .at = m_pipeCenter});
}

void Session::checkCheckpointAndGoal()
{
    const Area& area = currentArea();
    const Rect& box = m_hero.body.box;
    if (area.checkpoint.x >= 0 && !m_checkpointReached && m_area == 0) {
        const Rect flag = gamelib::core::cellRect(area.checkpoint, kTile);
        const Rect reach{flag.x + 3.0f, flag.y - 2.0f * kTile, flag.width - 6.0f, 3.0f * kTile};
        if (gamelib::core::overlaps(reach, box)) {
            m_checkpointReached = true;
            m_events.push({.kind = EventKind::Checkpoint, .at = area.map.cellBottom(area.checkpoint)});
            if (m_hero.power == Power::Small) {
                m_hero.growFrom = Power::Small;
                setPower(Power::Big);
                m_phase = Phase::Growing;
                m_phaseTicks = 0;
                m_events.push({.kind = EventKind::PowerUp, .at = box.center(), .item = ItemKind::Mushroom});
            }
        }
    }
    if (area.goal.x >= 0) {
        const Vec2 foot = area.map.cellBottom(area.goal);
        const float poleTop = foot.y - kPoleHeight;
        if (box.right() >= foot.x - 2.0f && box.left() <= foot.x + 2.0f && box.bottom() > poleTop) {
            const float height = std::clamp((box.bottom() - poleTop) / kPoleHeight, 0.0f, 1.0f);
            const int points = height < 0.12f ? 5000 : height < 0.35f ? 2000 : height < 0.55f ? 800 : height < 0.8f ? 400 : 100;
            m_phase = Phase::Goal;
            m_phaseTicks = 0;
            m_goalFlag = 0.0f;
            m_hero.body.velocity = {};
            m_hero.body.box.x = foot.x - 1.0f - m_hero.body.box.width;
            m_hero.previous = m_hero.body.box;
            m_shots.clear();
            m_events.push({.kind = EventKind::GoalTouch, .at = m_hero.body.box.center(), .value = points});
            addPoints(points, m_hero.body.box.center());
        }
    }
}

void Session::goalTick(const Input&)
{
    gamelib::core::PlatformerBody& body = m_hero.body;
    const gamelib::core::TileWorld world = tileWorld();
    const Vec2 foot = map().cellBottom(currentArea().goal);
    m_goalFlag = std::min(1.0f, m_goalFlag + 1.0f / 50.0f);
    if (m_phaseTicks < 56) {
        // Slide down the pole.
        const gamelib::core::BoxMove move = gamelib::core::moveBox(body.box, {0.0f, 2.2f}, world);
        body.box = move.box;
        body.grounded = move.contacts.ground;
        return;
    }
    if (m_phaseTicks == 56) {
        body.facing = 1;
        body.velocity = {};
    }
    // Walk off into the castle.
    gamelib::core::PlatformerTuning t = tuningFor(false, body.velocity.x, false, false);
    const gamelib::core::PlatformerStep step =
        gamelib::core::stepPlatformer(body, {.move = 1.0f, .climb = 0.0f, .jumpPressed = false, .jumpHeld = false}, t, world);
    (void)step;
    m_hero.walked += std::abs(body.box.x - m_hero.previous.x);
    if (m_phaseTicks >= 56 + kGoalWalkTicks || body.box.left() > foot.x + 7.0f * kTile) {
        addPoints(m_time * kTimeBonusPerUnit, body.box.center() + Vec2{0.0f, -24.0f});
        m_time = 0;
        m_events.push({.kind = EventKind::Clear, .at = body.box.center()});
        m_outcome = Outcome::Cleared;
        m_phase = Phase::Over;
        m_phaseTicks = 0;
    }
}

void Session::throwFireball()
{
    const Rect& box = m_hero.body.box;
    const float dir = static_cast<float>(m_hero.body.facing);
    Shots::Shot shot{.position = {box.center().x + dir * 6.0f, box.top() + 10.0f},
                     .velocity = {dir * 270.0f, 60.0f},
                     .size = {7.0f, 7.0f},
                     .lifetimeTicks = 300,
                     .team = 0,
                     .owner = 0,
                     .damage = 1,
                     .tileContact = gamelib::core::TileContact::BounceOnFloor,
                     .gravity = 1100.0f,
                     .bounceSpeed = 200.0f};
    if (m_shots.spawn(shot, 2) != 0) {
        m_hero.throwTicks = 10;
        m_events.push({.kind = EventKind::Fireball, .at = shot.position});
    }
}

void Session::stepShots()
{
    const gamelib::core::TileWorld world = tileWorld();
    m_shots.step(kTickSeconds, [&](const Rect& r) { return _shotSolid(world, r); }, map().bounds());
    for (const auto& removed : m_shots.removed()) {
        if (removed.reason == gamelib::core::ProjectileEnd::HitSolid) {
            m_events.push({.kind = EventKind::FireballPop, .at = removed.projectile.position});
        }
    }
    Area& area = currentArea();
    for (auto [handle, enemy] : area.enemies) {
        if (!enemy.awake || enemy.state == EnemyState::Knocked || enemy.state == EnemyState::Squashed) {
            continue;
        }
        enemy.vulnerable.setHurtbox(enemy.box);
        while (const std::optional<gamelib::core::ProjectileHit<BossShotData>> hit = m_shots.hitTest(enemy.vulnerable)) {
            m_events.push({.kind = EventKind::FireballPop, .at = hit->projectile.position});
            enemy.vulnerable.forgetAttack();
            if (enemy.kind == EnemyKind::Brute || enemy.kind == EnemyKind::King) {
                hitBoss(enemy, 1, hit->projectile.position);
            } else {
                knockOut(enemy, hit->projectile.velocity.x > 0.0f ? 1 : -1);
                addPoints(kFireballKillPoints, enemy.box.center());
            }
            break;
        }
    }
    if (m_hero.starTicks == 0) {
        m_hero.vulnerable.setHurtbox(m_hero.body.box.expanded(-2.0f));
        if (m_shots.hitTest(m_hero.vulnerable)) {
            m_hero.vulnerable.forgetAttack();
            hurtHero();
        }
    }
}

void Session::stepItems()
{
    Area& area = currentArea();
    const gamelib::core::TileWorld world = tileWorld();
    for (Item& item : area.items) {
        ++item.age;
        if (item.rise > 0) {
            item.box.y -= kTile / static_cast<float>(kItemRiseTicks);
            --item.rise;
            continue;
        }
        if (item.kind == ItemKind::Flower) {
            continue;
        }
        const float speed = item.kind == ItemKind::Star ? 1.3f : 1.0f;
        item.velocity.x = static_cast<float>(item.dir) * speed;
        item.velocity.y = std::min(item.velocity.y + kEnemyGravity, kEnemyMaxFall);
        const gamelib::core::BoxMove move = gamelib::core::moveBox(item.box, item.velocity, world);
        item.box = move.box;
        if (move.contacts.blockedX) {
            item.dir = -item.dir;
        }
        if (move.contacts.blockedY) {
            item.velocity.y = 0.0f;
        }
        if (item.kind == ItemKind::Star && move.contacts.ground) {
            item.velocity.y = -4.0f;
        }
    }
    // Taking an item.
    for (auto it = area.items.begin(); it != area.items.end();) {
        if (it->rise > 4 || !gamelib::core::overlaps(it->box, m_hero.body.box)) {
            ++it;
            continue;
        }
        const ItemKind kind = it->kind;
        const Vec2 at = it->box.center();
        it = area.items.erase(it);
        switch (kind) {
        case ItemKind::OneUp: addLife(at); break;
        case ItemKind::Star:
            m_hero.starTicks = kStarTicks;
            addPoints(kPowerUpPoints, at);
            m_events.push({.kind = EventKind::PowerUp, .at = at, .item = kind});
            break;
        case ItemKind::Mushroom:
        case ItemKind::Flower: {
            addPoints(kPowerUpPoints, at);
            m_events.push({.kind = EventKind::PowerUp, .at = at, .item = kind});
            const Power target = kind == ItemKind::Flower ? Power::Fire : (m_hero.power == Power::Small ? Power::Big : m_hero.power);
            if (target != m_hero.power) {
                m_hero.growFrom = m_hero.power;
                setPower(target);
                m_phase = Phase::Growing;
                m_phaseTicks = 0;
                return;
            }
            break;
        }
        }
    }
    std::erase_if(area.items, [&](const Item& item) { return item.box.top() > area.map.bounds().bottom(); });
}

void Session::hurtHero()
{
    if (m_phase != Phase::Playing || m_hero.starTicks > 0 || m_hero.vulnerable.invincible()) {
        return;
    }
    if (m_hero.power == Power::Small) {
        die();
        return;
    }
    m_hero.growFrom = m_hero.power;
    setPower(Power::Small);
    m_hero.vulnerable.makeInvincible(kHurtInvincibleTicks + kGrowTicks);
    m_phase = Phase::Growing;
    m_phaseTicks = 0;
    m_events.push({.kind = EventKind::Hurt, .at = m_hero.body.box.center()});
}

void Session::die()
{
    if (m_phase == Phase::Dying || m_phase == Phase::Over) {
        return;
    }
    m_phase = Phase::Dying;
    m_phaseTicks = 0;
    m_run.lives = std::max(0, m_run.lives - 1);
    m_run.power = Power::Small;
    m_hero.starTicks = 0;
    m_hero.body.velocity = {};
    m_shots.clear();
    const Vec2 at = m_hero.body.box.center();
    m_events.push({.kind = EventKind::Death, .at = {at.x, std::min(at.y, map().bounds().bottom() - 8.0f)}});
}

void Session::dyingTick()
{
    gamelib::core::PlatformerBody& body = m_hero.body;
    if (m_phaseTicks == 30) {
        body.velocity.y = -4.5f;
    }
    if (m_phaseTicks > 30) {
        body.box.y += body.velocity.y;
        body.velocity.y = std::min(body.velocity.y + 0.22f, 6.0f);
    }
    if (m_phaseTicks >= kDeathTicks) {
        m_outcome = Outcome::Died;
        m_phase = Phase::Over;
        m_phaseTicks = 0;
    }
}

void Session::addPoints(const int points, const Vec2 at)
{
    m_run.score += points;
    m_levelPoints += points;
    m_events.push({.kind = EventKind::Points, .at = at, .value = points});
}

void Session::addChainPoints(int& chain, const Vec2 at)
{
    const int points = kChainPoints[static_cast<std::size_t>(std::min(chain, static_cast<int>(kChainPoints.size()) - 1))];
    ++chain;
    if (points == 0) {
        addLife(at);
    } else {
        addPoints(points, at);
    }
}

void Session::addCoin(const Vec2 at)
{
    m_run.score += kCoinPoints;
    m_levelPoints += kCoinPoints;
    if (++m_run.coins >= kCoinsPerLife) {
        m_run.coins -= kCoinsPerLife;
        addLife(at);
    }
}

void Session::addLife(const Vec2 at)
{
    m_run.lives = std::min(kMaxLives, m_run.lives + 1);
    m_events.push({.kind = EventKind::OneUp, .at = at});
    m_events.push({.kind = EventKind::Points, .at = at, .value = 0});
}

std::uint64_t Session::hash() const
{
    gamelib::core::StateHash h;
    h.add(m_level).add(m_area).add(static_cast<int>(m_phase)).add(m_ticks).add(m_time);
    h.add(m_hero.body.box.position()).add(m_hero.body.velocity).add(m_hero.body.grounded);
    h.add(static_cast<int>(m_hero.power)).add(m_run.lives).add(m_run.coins).add(m_run.score);
    for (const Area& area : m_areas) {
        for (const auto [handle, enemy] : area.enemies) {
            h.add(enemy.box.position()).add(static_cast<int>(enemy.state));
        }
    }
    return h.value();
}

} // namespace bounder::game
