#pragma once

// The demo player: a simple bot that drives the same controller as the
// player through a level, reading only what a player sees - the tile map
// and the enemy list. It runs right and jumps at walls, gaps and enemies
// ahead (holding the jump longer for wider gaps and taller walls), waits for
// plants to sink, and backs off and tries again when it is stuck.
// Deterministic: the same session gives the same inputs.

#include "rules.hpp"
#include "session.hpp"

namespace bounder::game {

class DemoBot {
public:
    [[nodiscard]] Input next(const Session& session);

private:
    int m_hold = 0;      // ticks left to keep jump held
    bool m_held = false; // jump was held last tick (a new jump needs a release first)
    int m_stuckTicks = 0;
    float m_bestX = 0.0f;
    int m_backoff = 0;   // ticks left walking back
    int m_retries = 0;
};

} // namespace bounder::game
