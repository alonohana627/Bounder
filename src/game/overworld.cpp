#include "overworld.hpp"

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
    {"t.t..m...t....t..t..t.t.",
     "..t.....m.......t....t..",
     "t.S---1---2.....t.t...t.",
     "..........|...t.........",
     "t..t......|.....m...t.t.",
     "......4---3...t.........",
     "..t...|.........t..m....",
     "......+-----F---->...t..",
     "..~~~.....t....t......t.",
     "t.~~~...m....t...~~~....",
     "..t...t....t.....~~~..t."},
    // World 2: DUNE DESERT
    {"..m.....t.....m.....m...",
     ".<---1.......m......t...",
     ".....|..........t.......",
     ".m...+---2---3.....m....",
     ".............|.....m..t.",
     "....t...m....|..........",
     "......4------+.....m....",
     "..m...|............t....",
     "......+---F--------->...",
     "..m.........~~~......m..",
     ".....t......~~~...t....."},
    // World 3: FROST PEAKS
    {"..t.....m...t.....t..m..",
     ".<---1---2.........m..t.",
     "..t......|....t.........",
     "..m......|....t....t....",
     ".........+---3---4......",
     "..t.............|...t...",
     ".....t..........|..m....",
     "......F---------+.......",
     "..t...|......t......m...",
     "......+------------->...",
     "..~~~.......m....t..t..."},
    // World 4: SKY REALM
    {"~~~~~..~~~~~~~...~~~~~~~",
     "~<---1~~~~~~~~~~~t~~~~~~",
     "~~~~.|.~~~~~~~~~~~~~~~~~",
     "~~~~.+---2---3......m~~~",
     "~~t..........|.......~~~",
     "~~~~..m......|.......~~~",
     "~~~~~~..~4---+..t...~~~~",
     "~~~~~~~..|.........~~~~~",
     "~~~~~~~..+---F---C..~~~~",
     "~~~~~~~~~~~~~~~~~~~~~~~~",
     "~~~~~~~~~~~~~~~~~~~~~~~~"},
}};

WorldMap _build(const int world)
{
    const MapRows& rows = kMaps[static_cast<std::size_t>(world)];
    const auto isNode = [](const char c) { return std::string_view{"S<>1234FC"}.contains(c); };
    const auto isPath = [](const char c) { return c == '-' || c == '|' || c == '+'; };
    WorldMap map{.world = world, .paths = gamelib::core::NodeMap::fromText(rows, isNode, isPath), .nodes = {}, .start = -1};
    const int first = world * kLevelsPerWorld;
    for (const Cell cell : map.paths.nodes()) {
        const char c = map.paths.cells().at(cell);
        switch (c) {
        case 'S': map.nodes.push_back({.kind = NodeKind::Start, .cell = cell}); break;
        case '<': map.nodes.push_back({.kind = NodeKind::PrevWorld, .cell = cell}); break;
        case '>': map.nodes.push_back({.kind = NodeKind::NextWorld, .cell = cell}); break;
        case 'F': map.nodes.push_back({.kind = NodeKind::Level, .cell = cell, .level = first + 4}); break;
        case 'C': map.nodes.push_back({.kind = NodeKind::Level, .cell = cell, .level = kCastleLevel}); break;
        default: map.nodes.push_back({.kind = NodeKind::Level, .cell = cell, .level = first + (c - '1')}); break;
        }
        if (c == 'S' || c == '<') {
            map.start = static_cast<int>(map.nodes.size()) - 1;
        }
    }
    if (map.start < 0) {
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
    return map.paths.nodeAt(cell);
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
    return map.paths.openCells(map.start, [&](const int node) {
        return nodeCleared(map.nodes[static_cast<std::size_t>(node)], map.world, cleared);
    });
}

std::optional<std::vector<Cell>> route(const WorldMap& map, const int from, const Direction direction,
                                       const IsCleared& cleared)
{
    return map.paths.route(from, direction, openCells(map, cleared));
}

} // namespace bounder::game
