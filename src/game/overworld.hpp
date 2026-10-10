#pragma once

// The overworld: one map per world, the level select. A map is a grid of
// text: nodes (the start, levels 1-4, the fortress, the castle, and gates to
// the next and previous worlds) joined by paths. A path opens once the node
// it leaves from is cleared (the hero can always walk back): GameLib's
// NodeMap reads the text and walks the paths; which node is which level
// stays here.
//
//   S  start      1 2 3 4  levels      F  fortress      C  final castle
//   >  gate to the next world (open once the fortress is cleared)
//   <  gate back to the previous world (the start of worlds 2-4)
//   -  |  +  paths     decoration: t tree  m rock  ~ water  . grass

#include "levels.hpp"

#include <gamelib/core/grid.hpp>
#include <gamelib/core/node_map.hpp>
#include <gamelib/core/math.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace bounder::game {

enum class NodeKind : std::uint8_t { Start, Level, NextWorld, PrevWorld };

struct MapNode {
    NodeKind kind = NodeKind::Level;
    gamelib::core::Cell cell;
    int level = -1; // the campaign level (Level nodes, fortress and castle included)
};

struct WorldMap {
    int world = 0;
    gamelib::core::NodeMap paths; // the text: nodes, paths and scenery
    std::vector<MapNode> nodes;  // paths.nodes()[i] is nodes[i]
    int start = 0;               // the node a world is entered at (S or <)
};

[[nodiscard]] const WorldMap& worldMap(int world);

// Whether a level is cleared, from the saved campaign.
using IsCleared = std::function<bool(int level)>;

// A node counts as cleared when it lets the hero walk on past it: the start
// and the gates back always, a level once cleared, the gate onwards once
// the world's fortress is.
[[nodiscard]] bool nodeCleared(const MapNode& node, int world, const IsCleared& cleared);

// The open cells of a map: steps from the start along open paths (-1 where
// the hero cannot go yet).
[[nodiscard]] gamelib::core::Grid<int> openCells(const WorldMap& map, const IsCleared& cleared);

// The cells walked from node `from` in `direction` up to the next node
// (the last cell), if that way is open.
[[nodiscard]] std::optional<std::vector<gamelib::core::Cell>> route(const WorldMap& map, int from,
                                                                    gamelib::core::Direction direction,
                                                                    const IsCleared& cleared);

// The node at a cell, or -1.
[[nodiscard]] int nodeAt(const WorldMap& map, gamelib::core::Cell cell);
// The node of a level on its world's map, or -1.
[[nodiscard]] int nodeOfLevel(const WorldMap& map, int level);

} // namespace bounder::game
