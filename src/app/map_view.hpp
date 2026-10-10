#pragma once

// Drawing the overworld: a world's map of grass, trees, rocks and water with
// its paths (open ones dotted, closed ones faint), the level, fortress and
// castle nodes (cleared ones flagged), the gates between worlds and the hero
// walking between nodes, plus the panel naming the node he stands on.
// Canvas units; presentation only.

#include "overworld.hpp"
#include "view.hpp"

#include <gamelib/core/campaign.hpp>
#include <gamelib/core/grid.hpp>
#include <gamelib/draw/draw_list.hpp>

namespace bounder::app {

struct MapDrawState {
    gamelib::core::Vec2 hero;  // the hero's position in map cells (fractional while walking)
    int facing = 1;
    bool walking = false;
    float walked = 0.0f;       // cells walked, for the walk cycle
    int node = -1;             // the node he stands on (-1 while walking)
    float time = 0.0f;
};

void drawMap(gamelib::draw::DrawList& list, const Shapes& shapes, const game::WorldMap& map,
             const gamelib::core::Grid<int>& open, const gamelib::core::Campaign& campaign, const MapDrawState& state,
             gamelib::core::Size canvas);

} // namespace bounder::app
