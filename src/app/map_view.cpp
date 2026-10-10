#include "map_view.hpp"

#include "palette.hpp"
#include "profile.hpp"

#include <gamelib/draw/text.hpp>
#include <gamelib/draw/vector_text.hpp>
#include <gamelib/fx/sprite_motion.hpp>

#include <cmath>
#include <string>

namespace bounder::app {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Color;
using gamelib::core::Rect;
using gamelib::core::Vec2;

constexpr float kCell = 64.0f;

Vec2 _origin(const game::WorldMap& map, const gamelib::core::Size canvas)
{
    return {(static_cast<float>(canvas.width) - static_cast<float>(map.paths.cells().width()) * kCell) / 2.0f, 128.0f};
}

std::string _levelLine(const gamelib::core::Campaign& campaign, const int level)
{
    const gamelib::core::LevelRecord& record = campaign.record(levelId(level));
    std::string line;
    if (record.bestScore) {
        line = "BEST " + std::to_string(*record.bestScore);
    } else {
        line = record.cleared ? "CLEARED" : "NOT CLEARED YET";
    }
    return line;
}

} // namespace

void drawMap(gamelib::draw::DrawList& list, const Shapes& shapes, const game::WorldMap& map,
             const gamelib::core::Grid<int>& open, const gamelib::core::Campaign& campaign, const MapDrawState& state,
             const gamelib::core::Size canvas)
{
    const game::Theme theme = game::worldTheme(map.world);
    const ThemePalette& pal = themePalette(theme);
    const Vec2 origin = _origin(map, canvas);
    list.setView({});
    list.setLayer(gamelib::draw::layers::Background);
    list.vignette({0.0f, 0.0f, static_cast<float>(canvas.width), static_cast<float>(canvas.height)},
                  pal[Role::SkyTop].scaled(0.8f), 1.2f, 0.5f);
    // The map's board with a shadow and a border.
    const Rect board{origin.x, origin.y, static_cast<float>(map.paths.cells().width()) * kCell,
                     static_cast<float>(map.paths.cells().height()) * kCell};
    const Rect shadow = board.expanded(14.0f);
    list.box({shadow.x + 8.0f, shadow.y + 10.0f, shadow.width, shadow.height}, Color{0.0f, 0.0f, 0.0f, 0.3f}, {.cornerRadius = 28.0f});
    list.box(board.expanded(14.0f), pal[Role::MapDetail].scaled(0.6f), {.cornerRadius = 28.0f});

    const auto cellRect = [&](const Cell c) {
        return Rect{origin.x + static_cast<float>(c.x) * kCell, origin.y + static_cast<float>(c.y) * kCell, kCell, kCell};
    };
    list.setLayer(gamelib::draw::layers::TilesBehind);
    // Water, or in the sky world a sea of cloud.
    const Color water = theme == game::Theme::Sky ? pal[Role::SkyLow] : Color{0.25f, 0.55f, 0.95f, 1.0f};
    const Color waterLight = theme == game::Theme::Sky ? Color{1.0f, 1.0f, 1.0f, 1.0f} : water.scaled(1.4f);
    for (int y = 0; y < map.paths.cells().height(); ++y) {
        for (int x = 0; x < map.paths.cells().width(); ++x) {
            const Cell c{x, y};
            const char ch = map.paths.cells().at(c);
            if (ch == '~') {
                list.add({.rect = cellRect(c), .color = water, .color2 = waterLight, .shape = shapes.map,
                          .params = {3.0f, 0.0f, 0.0f, 0.0f}});
            } else {
                list.add({.rect = cellRect(c), .color = pal[Role::MapGround], .color2 = pal[Role::MapDetail],
                          .shape = shapes.map, .params = {0.0f, 0.0f, 0.0f, 0.0f}});
            }
        }
    }
    list.setLayer(gamelib::draw::layers::TilesBehind + 10);
    for (int y = 0; y < map.paths.cells().height(); ++y) {
        for (int x = 0; x < map.paths.cells().width(); ++x) {
            const Cell c{x, y};
            const char ch = map.paths.cells().at(c);
            if (ch == 't') {
                list.add({.rect = cellRect(c), .color = pal[Role::Bush], .color2 = pal[Role::BushLight], .shape = shapes.map,
                          .params = {1.0f, 0.0f, 0.0f, 0.0f}});
            } else if (ch == 'm') {
                list.add({.rect = cellRect(c), .color = pal[Role::Block], .color2 = pal[Role::Block].scaled(1.3f),
                          .shape = shapes.map, .params = {2.0f, 0.0f, 0.0f, 0.0f}});
            }
            if (!map.paths.isPath(c) && map.paths.nodeAt(c) < 0) {
                continue;
            }
            const std::uint64_t sides = map.paths.joins(c); // N, E, S, W bits
            const bool isOpen = open.at(c) >= 0;
            list.add({.rect = cellRect(c), .color = gamelib::core::srgb(0xF4E6C2), .color2 = gamelib::core::srgb(0xB8925A),
                      .shape = shapes.map, .params = {4.0f, isOpen ? 1.0f : 0.0f, 0.0f, 0.0f}, .bits = sides});
        }
    }
    // Nodes.
    list.setLayer(gamelib::draw::layers::Entities - 10);
    for (const game::MapNode& node : map.nodes) {
        const Rect r = cellRect(node.cell);
        switch (node.kind) {
        case game::NodeKind::Start:
            list.add({.rect = r, .color = gamelib::core::srgb(0xFFFFFF), .color2 = kHeroCap, .shape = shapes.map,
                      .params = {9.0f, 0.0f, 0.0f, 0.0f}});
            break;
        case game::NodeKind::PrevWorld:
        case game::NodeKind::NextWorld: {
            const bool next = node.kind == game::NodeKind::NextWorld;
            const bool reachable = open.at(node.cell) >= 0;
            list.add({.rect = r, .color = reachable ? gamelib::core::srgb(0xFFD23F) : gamelib::core::srgb(0x9AA0B0),
                      .color2 = gamelib::core::srgb(0xFFFFFF), .shape = shapes.map,
                      .params = {8.0f, 0.0f, next ? 1.0f : -1.0f, 0.0f}});
            break;
        }
        case game::NodeKind::Level: {
            const bool cleared = campaign.record(levelId(node.level)).cleared;
            const bool reachable = open.at(node.cell) >= 0;
            const bool fortress = game::isFortress(node.level);
            const bool castle = node.level == game::kCastleLevel;
            Color c = reachable ? gamelib::core::srgb(0xFF8A3D) : gamelib::core::srgb(0x9AA0B0);
            if (fortress || castle) {
                c = reachable ? gamelib::core::srgb(0xB8B0C8) : gamelib::core::srgb(0x8A8698);
            }
            const Color flag = cleared ? gamelib::core::srgb(0x35B54A) : kHeroCap;
            list.add({.rect = r, .color = c, .color2 = cleared ? gamelib::core::srgb(0x4CCB3F) : flag, .shape = shapes.map,
                      .params = {castle ? 7.0f : (fortress ? 6.0f : 5.0f), cleared ? 1.0f : 0.0f, 0.0f, 0.0f}});
            if (!fortress && !castle) {
                list.text(std::to_string(node.level % game::kLevelsPerWorld + 1), r.center().x, r.center().y - 12.0f,
                          {.pixelSize = 4.0f, .color = kText, .align = gamelib::draw::Align::Center, .shadow = true});
            }
            // Star coins found, as three small dots under the node.
            const std::uint64_t found = campaign.record(levelId(node.level)).found;
            for (int i = 0; i < 3; ++i) {
                const bool have = (found & (1u << i)) != 0;
                list.circle({r.center().x + static_cast<float>(i - 1) * 14.0f, r.bottom() - 2.0f}, 5.0f,
                            have ? kCoin : Color{0.0f, 0.0f, 0.0f, 0.35f}, {.ringWidth = have ? 0.0f : 1.5f});
            }
            break;
        }
        }
    }
    // The hero.
    list.setLayer(gamelib::draw::layers::Entities);
    const Vec2 at{origin.x + (state.hero.x + 0.5f) * kCell, origin.y + (state.hero.y + 0.5f) * kCell};
    const float hop = state.walking ? std::abs(std::sin(state.walked * 3.14159f)) * 6.0f : gamelib::fx::bob(state.time, 3.0f, 1.2f);
    // The level's small hero (16 x 16.5 units plus a 4-unit margin) at three times the size.
    const Rect hero{at.x - 36.0f, at.y - 64.0f - hop, 72.0f, 73.5f};
    list.add({.rect = hero, .color = kHeroCap, .color2 = kHeroOveralls, .shape = shapes.hero,
              .params = {static_cast<float>(state.facing), state.walking ? 1.0f : 0.0f,
                         gamelib::fx::walkPhase(state.walked, 1.0f), 4.0f},
              .scale = 3.0f});

    // The panel: the world, and what stands at the hero's node.
    list.setLayer(gamelib::draw::layers::Hud);
    const float cx = static_cast<float>(canvas.width) / 2.0f;
    gamelib::draw::drawVectorText(list, "WORLD " + std::to_string(map.world + 1) + "   " + std::string{game::worldName(map.world)},
                                  cx, 38.0f, {.size = 56.0f, .color = kText, .weight = 1.4f, .align = gamelib::draw::Align::Center});
    std::string title;
    std::string line;
    if (state.node >= 0) {
        const game::MapNode& node = map.nodes[static_cast<std::size_t>(state.node)];
        switch (node.kind) {
        case game::NodeKind::Start: title = "START"; break;
        case game::NodeKind::PrevWorld: title = "BACK TO WORLD " + std::to_string(map.world); break;
        case game::NodeKind::NextWorld:
            title = "ON TO WORLD " + std::to_string(map.world + 2);
            line = open.at(node.cell) >= 0 ? "PRESS JUMP TO TRAVEL" : "";
            break;
        case game::NodeKind::Level:
            title = std::string{game::level(node.level).id} + "   " + std::string{game::level(node.level).name};
            line = _levelLine(campaign, node.level) + "    PRESS JUMP TO PLAY";
            break;
        }
    }
    const float panelY = board.bottom() + 34.0f;
    list.box({cx - 620.0f, panelY, 1240.0f, 118.0f}, kPanel.withAlpha(0.88f),
             {.cornerRadius = 20.0f, .borderWidth = 4.0f, .borderColor = pal[Role::Accent]});
    list.text(title, cx, panelY + 22.0f, {.pixelSize = 6.0f, .color = kText, .align = gamelib::draw::Align::Center, .shadow = true});
    list.text(line, cx, panelY + 76.0f, {.pixelSize = 4.0f, .color = kTextDim, .align = gamelib::draw::Align::Center});
}

} // namespace bounder::app
