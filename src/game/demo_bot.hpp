#pragma once

// The demo player: a simple bot that drives the same controller as the
// player through a level. It runs right and, every couple of ticks on the
// ground, tries a handful of plans - keep running, jump now holding the
// button for 4 to 26 ticks, or wait a moment (for a moving platform, a
// plant, a hopper) - on a copy of the session for a second and a half of
// game time, and takes the plan that gets furthest without dying. So it
// jumps at gaps, walls and enemies ahead as the tile map and the enemy list
// make them, without knowing the level. Deterministic: the same session
// gives the same inputs.

#include "rules.hpp"
#include "session.hpp"

namespace bounder::game {

class DemoBot {
public:
    [[nodiscard]] Input next(const Session& session);

private:
    struct Plan {
        int jumpTicks = 0; // hold jump this long from now (0: no jump)
        int waitTicks = 0; // stand still this long first
    };

    [[nodiscard]] float rate(const Session& session, const Plan& plan) const;

    int m_hold = 0;      // ticks left to keep jump held
    int m_wait = 0;      // ticks left standing still
    bool m_held = false; // jump was held last tick (a new jump needs a release first)
    int m_think = 0;     // ticks until the next look ahead
    int m_stuckTicks = 0;
    float m_bestX = 0.0f;
    int m_backoff = 0;   // ticks left walking back
};

} // namespace bounder::game
