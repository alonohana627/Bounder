#include "view.hpp"

#include "palette.hpp"

#include "bushes_frag.spv.hpp"
#include "enemy_frag.spv.hpp"
#include "hero_frag.spv.hpp"
#include "item_frag.spv.hpp"
#include "map_frag.spv.hpp"
#include "scenery_frag.spv.hpp"
#include "sky_frag.spv.hpp"
#include "tile_frag.spv.hpp"

#include <gamelib/core/collision.hpp>
#include <gamelib/draw/parallax.hpp>
#include <gamelib/fx/sprite_motion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace bounder::app {

namespace {

// helpers

using gamelib::core::Color;
using gamelib::core::Rect;
using gamelib::core::Vec2;
using game::kTile;

constexpr float kPi = std::numbers::pi_v<float>;

enum TileKind : int {
    kGround = 0,
    kHard = 1,
    kBrick = 2,
    kQuestionBlock = 3,
    kUsedBlock = 4,
    kPipeTop = 5,
    kPipeBody = 6,
    kOneWay = 7,
    kSlope = 8, // 8..13
    kCoinTile = 14,
    kSpikes = 15,
    kLava = 16,
    kRailAcross = 17,
    kRailUp = 18,
};

gamelib::draw::TileSet _tileSet(const Shapes& shapes, const game::Theme theme)
{
    const ThemePalette& pal = themePalette(theme);
    const float t = themeIndex(theme);
    gamelib::draw::TileSet set;
    const auto style = [&](const char id, const int kind, const Color c, const Color c2, const std::uint8_t group,
                           const float z = 0.0f) {
        set.set(gamelib::core::charTile(id), {.shape = shapes.tile, .color = c, .color2 = c2,
                                              .params = {static_cast<float>(kind), t, z, 0.0f}, .group = group});
    };
    style('#', kGround, pal[Role::Ground], pal[Role::GroundCap], 0);
    const std::array<char, 6> slopes{'/', '\\', 'a', 'A', 'D', 'd'};
    for (std::size_t i = 0; i < slopes.size(); ++i) {
        style(slopes[i], kSlope + static_cast<int>(i), pal[Role::Ground], pal[Role::GroundCap], 0);
    }
    style('X', kHard, pal[Role::Block], pal[Role::Block], 1);
    style('B', kBrick, pal[Role::Brick], pal[Role::Brick], 2);
    style('C', kBrick, pal[Role::Brick], pal[Role::Brick], 2);
    style('?', kQuestionBlock, kQuestion, kQuestionMark, 3);
    style('P', kQuestionBlock, kQuestion, kQuestionMark, 3);
    style('*', kQuestionBlock, kQuestion, kQuestionMark, 3);
    style('u', kUsedBlock, kUsed, kUsed, 3);
    style('{', kPipeTop, pal[Role::Pipe], pal[Role::Pipe], 4, 0.0f);
    style('E', kPipeTop, pal[Role::Pipe], pal[Role::Pipe], 4, 0.0f);
    style('O', kPipeTop, pal[Role::Pipe], pal[Role::Pipe], 4, 0.0f);
    style('}', kPipeTop, pal[Role::Pipe], pal[Role::Pipe], 4, 1.0f);
    style('[', kPipeBody, pal[Role::Pipe], pal[Role::Pipe], 4, 0.0f);
    style(']', kPipeBody, pal[Role::Pipe], pal[Role::Pipe], 4, 1.0f);
    style('=', kOneWay, pal[Role::Platform], pal[Role::PlatformTop], 5);
    style('o', kCoinTile, kCoin, kCoinLight, 6);
    style('^', kSpikes, gamelib::core::srgb(0xC8CCD8), gamelib::core::srgb(0xFFFFFF), 7);
    style('~', kLava, gamelib::core::srgb(0xFF4A12), gamelib::core::srgb(0xFFC23A), 8);
    style('-', kRailAcross, pal[Role::Ground].scaled(0.5f).withAlpha(0.8f), pal[Role::Ground], 9);
    style('|', kRailUp, pal[Role::Ground].scaled(0.5f).withAlpha(0.8f), pal[Role::Ground], 9);
    return set;
}

Rect _lerp(const Rect& a, const Rect& b, const float t)
{
    return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.width, b.width, t), std::lerp(a.height, b.height, t)};
}

// A drawing rectangle of `size` standing on the box's bottom center, grown by `margin`.
Rect _standing(const Rect& box, const Vec2 size, const float margin)
{
    return {box.center().x - size.x / 2.0f - margin, box.bottom() - size.y - margin, size.x + 2.0f * margin,
            size.y + 2.0f * margin};
}

struct EnemyLook {
    float kind = 0.0f;
    Vec2 size;
    Color body;
    Color accent;
};

EnemyLook _look(const game::Enemy& e)
{
    using game::EnemyKind;
    const bool shell = e.state == game::EnemyState::ShellIdle || e.state == game::EnemyState::ShellSliding;
    switch (e.kind) {
    case EnemyKind::Walker: return {0.0f, {18.0f, 16.0f}, gamelib::core::srgb(0xA8582E), gamelib::core::srgb(0xF2D3A0)};
    case EnemyKind::Shell:
        return {shell ? 2.0f : 1.0f, shell ? Vec2{18.0f, 13.0f} : Vec2{18.0f, 22.0f}, gamelib::core::srgb(0x35B54A),
                gamelib::core::srgb(0xF5E9B0)};
    case EnemyKind::RedShell:
        return {shell ? 2.0f : 1.0f, shell ? Vec2{18.0f, 13.0f} : Vec2{18.0f, 22.0f}, gamelib::core::srgb(0xE8343B),
                gamelib::core::srgb(0xF5E9B0)};
    case EnemyKind::Hopper: return {3.0f, {18.0f, 18.0f}, gamelib::core::srgb(0x5CC94A), gamelib::core::srgb(0xF5F0B0)};
    case EnemyKind::Flier:
    case EnemyKind::FlierVertical: return {4.0f, {28.0f, 16.0f}, gamelib::core::srgb(0xC06AE8), gamelib::core::srgb(0xFFD8F4)};
    case EnemyKind::Plant: return {5.0f, {16.0f, 24.0f}, gamelib::core::srgb(0xE8343B), gamelib::core::srgb(0xFFFFFF)};
    case EnemyKind::Spiky: return {6.0f, {22.0f, 17.0f}, gamelib::core::srgb(0xE8343B), gamelib::core::srgb(0xFFFFFF)};
    case EnemyKind::Brute: return {7.0f, {38.0f, 36.0f}, gamelib::core::srgb(0x7A4AC8), gamelib::core::srgb(0xD8C0FF)};
    case EnemyKind::King: return {8.0f, {50.0f, 52.0f}, gamelib::core::srgb(0x3E8E3A), gamelib::core::srgb(0xF2E2A0)};
    }
    return {};
}

} // namespace

Shapes Shapes::registerWith(gamelib::gfx::Renderer& renderer)
{
    return {.tile = renderer.addShape(shaders::tile_frag),
            .hero = renderer.addShape(shaders::hero_frag),
            .enemy = renderer.addShape(shaders::enemy_frag),
            .item = renderer.addShape(shaders::item_frag),
            .scenery = renderer.addShape(shaders::scenery_frag),
            .sky = renderer.addShape(shaders::sky_frag),
            .bushes = renderer.addShape(shaders::bushes_frag),
            .map = renderer.addShape(shaders::map_frag)};
}

LevelView::LevelView(const gamelib::core::Size canvas)
    : m_canvas{canvas}
    , m_camera{canvas, kZoom}
{
}

void LevelView::snap(const game::Session& session)
{
    m_area = session.areaIndex();
    m_locked = false;
    m_camera.setBounds(session.map().bounds());
    m_camera.centerOn(session.hero().body.box.center() + Vec2{40.0f, 0.0f});
}

void LevelView::follow(const game::Session& session, const float alpha, const float seconds)
{
    if (session.areaIndex() != m_area) {
        snap(session);
    }
    if (const std::optional<Rect> arena = session.lockedArena(); arena && !m_locked) {
        m_locked = true;
        m_camera.setBounds(*arena, 0.8f);
    }
    const game::Hero& hero = session.hero();
    const Rect box = _lerp(hero.previous, hero.body.box, alpha);
    const bool goal = session.phase() == game::Phase::Goal || session.phase() == game::Phase::Dying
                      || session.phase() == game::Phase::Over;
    if (session.phase() == game::Phase::Dying || session.phase() == game::Phase::Over) {
        return; // the camera holds while the hero falls away
    }
    m_camera.follow({.position = box.center(), .facing = goal ? 0.0f : static_cast<float>(hero.body.facing),
                     .grounded = hero.body.grounded},
                    seconds,
                    {.deadZone = {28.0f, 44.0f}, .lookahead = 48.0f, .lookaheadRate = 1.6f, .rate = 7.0f,
                     .verticalOnLanding = true, .maxRise = 70.0f});
}

void LevelView::draw(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                     const Effects& effects, const LevelDrawOptions& options) const
{
    list.setView({});
    drawBackdrop(list, shapes, session);
    list.setView(m_camera.view(options.shake));
    drawTiles(list, shapes, session);
    drawFurniture(list, shapes, session, options.alpha, options.foundBefore);
    drawActors(list, shapes, session, effects, options.alpha);
    drawHero(list, shapes, session, effects, options.alpha);
    list.setLayer(gamelib::draw::layers::Effects);
    effects.drawWorld(list, shapes);
}

void LevelView::drawBackdrop(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const
{
    const game::Theme theme = session.area().theme;
    const ThemePalette& pal = themePalette(theme);
    const float t = themeIndex(theme);
    const gamelib::core::View2D camera = m_camera.view();
    list.setLayer(gamelib::draw::layers::Background);
    const bool indoors = theme == game::Theme::Fortress || theme == game::Theme::Castle || theme == game::Theme::Bonus;
    // The far layers stand on the level's ground line (two rows above the
    // map's bottom) and sink by their own factor when the camera climbs, so
    // they scroll vertically slower than the tiles in tall levels.
    const float climb = (session.map().bounds().bottom() - game::kViewHeight - camera.origin.y) * kZoom; // canvas units above the bottom
    const float ground = 2.0f * kTile * kZoom;
    const auto rest = [&](const float factor, const float lift) { return ground + lift - climb * factor; };
    std::vector<gamelib::draw::ParallaxLayer> layers;
    layers.push_back({.shape = shapes.sky, .factor = {0.2f, 0.0f}, .offset = {0.0f, -climb * 0.1f},
                      .color = pal[Role::SkyTop], .color2 = pal[Role::SkyLow], .params = {t, 0.0f}});
    if (!indoors) {
        // Hills: the ridge base sits at params.z of the quad, scrolled by the offset.
        if (theme != game::Theme::Sky) {
            layers.push_back({.shape = gamelib::draw::shapes::Hills, .factor = {0.12f, 0.0f}, .offset = {0.0f, rest(0.1f, 60.0f)},
                              .color = lerp(pal[Role::Hills], pal[Role::SkyLow], 0.45f), .color2 = lerp(pal[Role::HillsRidge], pal[Role::SkyLow], 0.35f),
                              .params = {0.6f, 160.0f}, .bits = 11 + static_cast<std::uint64_t>(theme)});
        }
        layers.push_back({.shape = gamelib::draw::shapes::Clouds, .factor = {0.06f, 0.0f}, .offset = {0.0f, -climb * 0.05f},
                          .color = pal[Role::Cloud].withAlpha(theme == game::Theme::Sky ? 0.95f : 0.9f),
                          .color2 = pal[Role::CloudShade], .params = {theme == game::Theme::Sky ? 0.55f : 0.35f, 260.0f},
                          .bits = 5});
        if (theme != game::Theme::Sky) {
            layers.push_back({.shape = gamelib::draw::shapes::Hills, .factor = {0.28f, 0.0f}, .offset = {0.0f, rest(0.25f, 0.0f)},
                              .color = pal[Role::Hills], .color2 = pal[Role::HillsRidge], .params = {0.78f, 80.0f},
                              .bits = 29 + static_cast<std::uint64_t>(theme)});
        }
    }
    if (theme != game::Theme::Bonus) {
        layers.push_back({.shape = shapes.bushes, .factor = {0.5f, 0.0f}, .offset = {0.0f, rest(0.45f, 0.0f)},
                          .color = pal[Role::Bush], .color2 = pal[Role::BushLight], .params = {t, 0.0f}});
    }
    gamelib::draw::drawParallax(list, layers, camera, m_canvas);
}
void LevelView::drawTiles(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const
{
    const game::Area& area = session.area();
    const gamelib::draw::TileSet tiles = _tileSet(shapes, area.theme);
    const gamelib::core::View2D view = m_camera.view();
    const Rect visible = view.visibleWorld(m_canvas).expanded(kTile);
    list.setLayer(gamelib::draw::layers::TilesBehind);
    if (area.bumps.empty()) {
        gamelib::draw::drawTiles(list, area.map, 0, tiles, visible);
        return;
    }
    // Bumped blocks hop: draw the layer without them, then each one raised.
    gamelib::core::TileLayer layer = area.map.layer(0);
    for (const game::Bump& bump : area.bumps) {
        layer.at(bump.cell) = ' ';
    }
    gamelib::draw::drawTiles(list, layer, kTile, tiles, visible);
    for (const game::Bump& bump : area.bumps) {
        const gamelib::core::TileId id = area.map.layer(0).at(bump.cell);
        const gamelib::draw::TileStyle* style = tiles.find(id);
        if (style == nullptr) {
            continue;
        }
        const float t = static_cast<float>(bump.ticks) / 12.0f;
        const float lift = std::sin(t * kPi) * 5.0f;
        Rect rect = gamelib::core::cellRect(bump.cell, kTile);
        rect.y -= lift;
        list.add({.rect = rect, .color = style->color, .color2 = style->color2, .shape = style->shape,
                  .params = style->params,
                  .bits = (static_cast<std::uint64_t>(gamelib::draw::tileVariant(bump.cell)) << 8)
                          | (static_cast<std::uint64_t>(static_cast<std::uint16_t>(bump.cell.x)) << 32)
                          | (static_cast<std::uint64_t>(static_cast<std::uint16_t>(bump.cell.y)) << 48)});
    }
}

void LevelView::drawFurniture(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                              const float alpha, const std::uint8_t foundBefore) const
{
    const game::Area& area = session.area();
    const ThemePalette& pal = themePalette(area.theme);
    list.setLayer(gamelib::draw::layers::TilesBehind - 10);
    // The castle and the goal pole.
    if (area.goal.x >= 0) {
        const Vec2 foot = area.map.cellBottom(area.goal);
        const bool indoors = area.theme == game::Theme::Fortress || area.theme == game::Theme::Castle;
        if (!indoors) {
            list.add({.rect = {foot.x + 6.0f * kTile - 40.0f, foot.y - 108.0f, 80.0f, 108.0f},
                      .color = pal[Role::Block], .color2 = kHeroCap, .shape = shapes.scenery, .params = {2.0f, 0.0f, 0.0f, 0.0f}});
        }
        const float poleHeight = 9.5f * kTile;
        const float flag = session.phase() == game::Phase::Goal || session.phase() == game::Phase::Over ? session.goalFlag() : 0.0f;
        list.add({.rect = {foot.x - 16.0f, foot.y - poleHeight - 4.0f, 32.0f, poleHeight + 4.0f},
                  .color = gamelib::core::srgb(0xE8ECF4), .color2 = gamelib::core::srgb(0x35B54A), .shape = shapes.scenery,
                  .params = {0.0f, flag, 0.0f, 0.0f}});
    }
    if (area.checkpoint.x >= 0 && session.areaIndex() == 0) {
        const Vec2 foot = area.map.cellBottom(area.checkpoint);
        list.add({.rect = {foot.x - 8.0f, foot.y - 3.0f * kTile, 24.0f, 3.0f * kTile},
                  .color = gamelib::core::srgb(0x9AA0B0), .color2 = kHeroCap, .shape = shapes.scenery,
                  .params = {1.0f, session.checkpointReached() ? 1.0f : 0.0f, 0.0f, 0.0f}});
    }
    list.setLayer(gamelib::draw::layers::Entities - 20);
    for (const game::Platform& p : area.platforms) {
        const Rect box = _lerp(p.previous, p.box, alpha);
        const float shakeAmount = p.falling ? std::min(1.0f, static_cast<float>(p.standTicks) / 14.0f) : 0.0f;
        Rect drawn = box;
        drawn.height = kTile * 0.6f;
        if (p.falling && !p.dropping && p.standTicks > 0) {
            drawn.x += std::sin(static_cast<float>(p.standTicks) * 2.3f) * 0.7f;
        }
        list.add({.rect = drawn, .color = p.falling ? gamelib::core::srgb(0xD98A3A) : pal[Role::PlatformTop],
                  .color2 = gamelib::core::srgb(0x5A4A3A), .shape = shapes.scenery,
                  .params = {p.falling ? 4.0f : 3.0f, shakeAmount, 0.0f, 0.0f}});
    }
    for (const game::StarCoin& coin : session.starCoins()) {
        if (coin.taken || coin.area != session.areaIndex()) {
            continue;
        }
        const bool ghost = (foundBefore & (1u << coin.index)) != 0;
        list.add({.rect = Rect::fromCenter(coin.center, {22.0f, 22.0f}), .color = kCoin, .color2 = gamelib::core::srgb(0xFFFFFF),
                  .shape = shapes.item, .params = {4.0f, ghost ? 1.0f : 0.0f, 0.0f, 0.0f}});
    }
}

void LevelView::drawActors(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                           const Effects&, const float alpha) const
{
    const game::Area& area = session.area();
    // Items rising out of blocks are drawn behind the tiles.
    for (const game::Item& item : area.items) {
        list.setLayer(item.rise > 0 ? gamelib::draw::layers::TilesBehind - 5 : gamelib::draw::layers::Entities);
        const Rect box = _lerp(item.previous, item.box, alpha);
        Color c = kMushroom;
        Color c2 = gamelib::core::srgb(0xFFFFFF);
        float kind = 0.0f;
        switch (item.kind) {
        case game::ItemKind::Mushroom: break;
        case game::ItemKind::OneUp: c = kOneUp; kind = 3.0f; break;
        case game::ItemKind::Flower: c = kFlower; c2 = kFlowerInner; kind = 1.0f; break;
        case game::ItemKind::Star: c = kStar; c2 = kStarGlow; kind = 2.0f; break;
        }
        list.add({.rect = Rect::fromCenter(box.center(), {18.0f, 18.0f}), .color = c, .color2 = c2, .shape = shapes.item,
                  .params = {kind, 0.0f, 0.0f, 0.0f}});
    }
    list.setLayer(gamelib::draw::layers::Entities);
    const float time = static_cast<float>(session.ticks()) / static_cast<float>(game::kTickRate);
    for (const auto [handle, e] : area.enemies) {
        if (!e.awake && e.kind != game::EnemyKind::Brute && e.kind != game::EnemyKind::King) {
            // Asleep enemies wait off screen; draw them anyway if visible.
        }
        const EnemyLook look = _look(e);
        const Rect box = _lerp(e.previous, e.box, alpha);
        const float margin = 3.0f;
        Rect rect = _standing(box, look.size, margin);
        if (e.kind == game::EnemyKind::Flier || e.kind == game::EnemyKind::FlierVertical) {
            rect = Rect::fromCenter(box.center() + Vec2{0.0f, 2.0f}, look.size + Vec2{2.0f * margin, 2.0f * margin});
        }
        float phase = 0.0f;
        switch (e.kind) {
        case game::EnemyKind::Hopper: phase = e.grounded ? 0.0f : 1.0f; break;
        case game::EnemyKind::Plant: phase = std::fmod(time * 2.0f, 1.0f); break;
        case game::EnemyKind::Flier:
        case game::EnemyKind::FlierVertical: phase = std::fmod(time * 3.0f, 1.0f); break;
        default:
            phase = e.state == game::EnemyState::ShellSliding ? std::fmod(time * 6.0f, 1.0f)
                                                             : std::fmod(std::abs(box.x) / 14.0f, 1.0f);
            break;
        }
        std::uint64_t bits = 0;
        if (e.state == game::EnemyState::Squashed) {
            bits |= 1;
        }
        if (e.hurtTicks > 0) {
            bits |= 2;
        }
        if (e.kind == game::EnemyKind::King && e.hp <= 4) {
            bits |= 4;
        }
        const bool knocked = e.state == game::EnemyState::Knocked;
        if (e.kind == game::EnemyKind::Plant) {
            // The plant is drawn behind its pipe.
            list.setLayer(gamelib::draw::layers::TilesBehind - 5);
        }
        list.add({.rect = rect, .color = look.body, .color2 = look.accent, .shape = shapes.enemy,
                  .rotation = knocked ? kPi : 0.0f,
                  .params = {look.kind, static_cast<float>(e.dir), phase, margin}, .bits = bits});
        if (e.kind == game::EnemyKind::Plant) {
            list.setLayer(gamelib::draw::layers::Entities);
        }
    }
    // Fireballs.
    list.setLayer(gamelib::draw::layers::Entities + 10);
    for (const auto& shot : session.shots().items()) {
        const bool boss = shot.extra.boss;
        list.add({.rect = Rect::fromCenter(shot.position, boss ? Vec2{16.0f, 16.0f} : Vec2{12.0f, 12.0f}),
                  .color = boss ? gamelib::core::srgb(0x7AE04A) : kFire, .color2 = boss ? gamelib::core::srgb(0xE8FFB0) : kFireCore,
                  .shape = shapes.item, .params = {boss ? 6.0f : 5.0f, 0.0f, 0.0f, 0.0f}});
    }
}

void LevelView::drawHero(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                         const Effects& effects, const float alpha) const
{
    const game::Hero& hero = session.hero();
    const game::Phase phase = session.phase();
    if (!hero.vulnerable.visible(4) && phase == game::Phase::Playing) {
        return; // the flicker after a hit
    }
    const Rect box = _lerp(hero.previous, hero.body.box, alpha);
    if (phase == game::Phase::Goal || (phase == game::Phase::Over && session.outcome() == game::Outcome::Cleared)) {
        const game::Area& area = session.area();
        const bool indoors = area.theme == game::Theme::Fortress || area.theme == game::Theme::Castle;
        if (area.goal.x >= 0 && !indoors && box.center().x >= area.map.cellBottom(area.goal).x + 6.0f * kTile - 4.0f) {
            return; // in through the castle's door
        }
    }
    bool big = hero.power != game::Power::Small;
    if (phase == game::Phase::Growing && (session.phaseTicks() / 4) % 2 == 0) {
        big = hero.growFrom != game::Power::Small; // flickers between the old and the new size
    }
    game::Power look = hero.power;
    if (phase == game::Phase::Growing && (session.phaseTicks() / 4) % 2 == 0) {
        look = hero.growFrom;
    }
    const Vec2 size = big ? Vec2{16.0f, 28.0f} : Vec2{16.0f, 16.5f};
    const float margin = 4.0f;
    Rect rect = _standing(box, size, margin);
    // Squash and stretch around the feet.
    const Vec2 scale = phase == game::Phase::Playing ? effects.heroScale() : Vec2{1.0f, 1.0f};
    rect = gamelib::fx::scaledFromBottom(rect, scale);

    float pose = 0.0f;
    if (phase == game::Phase::Dying || (phase == game::Phase::Over && session.outcome() == game::Outcome::Died)) {
        pose = 5.0f;
    } else if (hero.throwTicks > 0) {
        pose = 6.0f;
    } else if (hero.skidding) {
        pose = 4.0f;
    } else if (!hero.body.grounded && phase != game::Phase::PipeIn && phase != game::Phase::PipeOut) {
        pose = hero.body.velocity.y < 0.0f ? 2.0f : 3.0f;
    } else if (std::abs(hero.body.velocity.x) > 0.15f || phase == game::Phase::Goal) {
        pose = 1.0f;
    }
    const float walk = gamelib::fx::walkPhase(hero.walked, big ? 22.0f : 18.0f);
    const bool fire = look == game::Power::Fire;
    std::uint64_t bits = big ? 1u : 0u;
    if (hero.starTicks > 0) {
        bits |= 0x100u;
    }
    // In a pipe the hero is drawn behind the pipe's tiles.
    list.setLayer(phase == game::Phase::PipeIn || phase == game::Phase::PipeOut ? gamelib::draw::layers::TilesBehind - 5
                                                                                 : gamelib::draw::layers::Entities + 5);
    list.add({.rect = rect, .color = fire ? kFireCap : kHeroCap, .color2 = fire ? kFireOveralls : kHeroOveralls,
              .shape = shapes.hero, .params = {static_cast<float>(hero.body.facing), pose, walk, margin}, .bits = bits});
}

} // namespace bounder::app
