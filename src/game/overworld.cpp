#include "overworld.hpp"

#include <gamelib/core/grid_search.hpp>

#include <array>
#include <stdexcept>
#include <string_view>

namespace bounder::game {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Direction;

using MapRows = std::array<std::string_view, 11>;

constexpr std::array<MapRows, kWorlds> kMaps{{
    // World 1: GRASSLAND
    {"........................",
     "..t.....m.......t.......",
     "..S---1---2...........t.",
     "..........|.............",
     "...t......|.....m.......",
     "......4---3.............",
     "......|.................",
     "......+-----F---->......",
     "..~~~..........t........",
     "..~~~...m...............",
     "........................"},
    // World 2: DUNE DESERT
    {"........................",
     ".<---1.......m..........",
     ".....|..........t.......",
     ".m...+---2---3..........",
     ".............|.....m....",
     "....t........|..........",
     "......4------+..........",
     "......|.................",
     "......+---F--------->...",
     "..m.........~~~.........",
     "............~~~...t....."},
    // World 3: FROST PEAKS
    {"........m...............",
     ".<---1---2.........m....",
     ".........|..............",
     "..m......|....t.........",
     ".........+---3---4......",
     "................|.......",
     ".....t..........|..m....",
     "......F---------+.......",
     "......|.................",
     "......+------------->...",
     "..~~~.......m..........."},
    // World 4: SKY REALM
    {"........................",
     ".<---1...........t......",
     ".....|..................",
     ".....+---2---3......m...",
     "..t..........|..........",
     "......m......|..........",
     ".........4---+..........",
     ".........|..............",
     ".........+---F---C......",
     "...~~~..................",
     "...~~~.......t.........."},
}};

bool _isPath(const char c)
{
    return c == '-' || c == '|' || c == '+';
}

WorldMap _build(const int world)
{
    const MapRows& rows = kMaps[static_cast<std::size_t>(world)];
    WorldMap map{.world = world, .tiles = gamelib::core::gridFromText(rows, [](const char c) { return c; }), .nodes = {}};
    const int first = world * kLevelsPerWorld;
    const auto add = [&](const MapNode node) {
        if (node.kind == NodeKind::Start || node.kind == NodeKind::PrevWorld) {
            map.nodes.insert(map.nodes.begin(), node);
        } else {
            map.nodes.push_back(node);
        }
    };
    for (int y = 0; y < map.tiles.height(); ++y) {
        for (int x = 0; x < map.tiles.width(); ++x) {
            const Cell cell{x, y};
            const char c = map.tiles.at(cell);
            switch (c) {
            case 'S': add({.kind = NodeKind::Start, .cell = cell}); break;
            case '<': add({.kind = NodeKind::PrevWorld, .cell = cell}); break;
            case '>': add({.kind = NodeKind::NextWorld, .cell = cell}); break;
            case '1':
            case '2':
            case '3':
            case '4': add({.kind = NodeKind::Level, .cell = cell, .level = first + (c - '1')}); break;
            case 'F': add({.kind = NodeKind::Level, .cell = cell, .level = first + 4}); break;
            case 'C': add({.kind = NodeKind::Level, .cell = cell, .level = kCastleLevel}); break;
            default: break;
            }
        }
    }
    if (map.nodes.empty() || (map.nodes.front().kind != NodeKind::Start && map.nodes.front().kind != NodeKind::PrevWorld)) {
        throw std::logic_error{"Bounder: a world map without a start"};
    }
    return map;
}

} // namespace

const WorldMap& worldMap(const int world)
{
    static const std::array<WorldMap, kWorlds> maps{_build(0), _build(1), _build(2), _build(3)};
    return maps.at(static_cast<std::size_t>(world));
}

bool nodeCleared(const MapNode& node, const int world, const IsCleared& cleared)
{
    switch (node.kind) {
    case NodeKind::Start:
    case NodeKind::PrevWorld: return true;
    case NodeKind::Level: return cleared(node.level);
    case NodeKind::NextWorld: return cleared(world * kLevelsPerWorld + 4);
    }
    return false;
}

int nodeAt(const WorldMap& map, const Cell cell)
{
    for (std::size_t i = 0; i < map.nodes.size(); ++i) {
        if (map.nodes[i].cell == cell) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int nodeOfLevel(const WorldMap& map, const int level)
{
    for (std::size_t i = 0; i < map.nodes.size(); ++i) {
        if (map.nodes[i].kind == NodeKind::Level && map.nodes[i].level == level) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

gamelib::core::Grid<int> openCells(const WorldMap& map, const IsCleared& cleared)
{
    const std::array<Cell, 1> start{map.nodes.front().cell};
    return gamelib::core::floodFill(map.tiles, start, gamelib::core::Neighbors::Four, [&](const Cell cell) {
        const int node = nodeAt(map, cell);
        if (node >= 0) {
            // A node is reached; only a cleared one lets the fill go on.
            return nodeCleared(map.nodes[static_cast<std::size_t>(node)], map.world, cleared)
                       ? gamelib::core::Reach::Spread
                       : gamelib::core::Reach::Stop;
        }
        return _isPath(map.tiles.at(cell)) ? gamelib::core::Reach::Spread : gamelib::core::Reach::Blocked;
    });
}

std::optional<std::vector<Cell>> route(const WorldMap& map, const int from, const Direction direction,
                                       const IsCleared& cleared)
{
    const gamelib::core::Grid<int> open = openCells(map, cleared);
    Cell at = map.nodes.at(static_cast<std::size_t>(from)).cell;
    Cell next = gamelib::core::neighbor(at, direction);
    if (!map.tiles.inside(next) || open.at(next) < 0) {
        return std::nullopt;
    }
    std::vector<Cell> cells;
    Cell previous = at;
    // Follow the path through its bends to the next node.
    for (int guard = 0; guard < map.tiles.width() * map.tiles.height(); ++guard) {
        cells.push_back(next);
        if (nodeAt(map, next) >= 0) {
            return cells;
        }
        bool moved = false;
        for (const Direction d : gamelib::core::kDirections) {
            const Cell candidate = gamelib::core::neighbor(next, d);
            if (candidate == previous || !map.tiles.inside(candidate) || open.at(candidate) < 0) {
                continue;
            }
            previous = next;
            next = candidate;
            moved = true;
            break;
        }
        if (!moved) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace bounder::game
