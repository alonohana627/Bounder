#pragma once

// The level texts, one file per world (levels_w1.cpp ... levels_castle.cpp),
// assembled by buildLevels() in campaign order. Internal to src/game.

#include "levels.hpp"

#include <string_view>
#include <vector>

namespace bounder::game {

using Segment = std::vector<std::string_view>;

// Shared segments: the end of an outdoor level (a staircase, the goal pole
// and room to walk into the castle) and a boss arena (its gate column 'z',
// the boss 'Z', two ledges).
extern const Segment kGoalSegment;
extern const Segment kArenaSegment;

[[nodiscard]] std::vector<LevelDef> worldOneLevels();
[[nodiscard]] std::vector<LevelDef> worldTwoLevels();
[[nodiscard]] std::vector<LevelDef> worldThreeLevels();
[[nodiscard]] std::vector<LevelDef> worldFourLevels();
[[nodiscard]] LevelDef castleLevel();
// A short fortress with its boss arena (until every world is written).
[[nodiscard]] LevelDef placeholderFortress(std::string_view id, int world);

// Every level in campaign order (4 x 5, then the castle).
[[nodiscard]] std::vector<LevelDef> buildLevels();

} // namespace bounder::game
