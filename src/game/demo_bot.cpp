#include "demo_bot.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace bounder::game {

namespace {

// helpers

constexpr int kHorizon = 90;   // ticks each plan is tried for
constexpr int kThinkEvery = 2; // ticks between looks ahead on the ground

constexpr std::array<int, 7> kJumps{0, 4, 8, 12, 16, 20, 26};
constexpr std::array<int, 2> kWaits{14, 36};

} // namespace

float DemoBot::rate(const Session& session, const Plan& plan) const
{
    // The plan: wait, then run right (jumping as planned) until the hero
    // lands again - or, without a jump, for half a second - then let go and
    // see where he comes to rest. Running on blindly would count every
    // ledge after the landing against the jump.
    Session trial = session;
    bool held = m_held;
    bool airborne = false;
    bool settled = false;
    for (int t = 0; t < kHorizon; ++t) {
        Input in;
        const int running = t - plan.waitTicks;
        if (!trial.hero().body.grounded) {
            airborne = true;
        } else if (airborne || (plan.jumpTicks == 0 && running >= 30)) {
            settled = true;
        }
        in.move = settled ? 0 : 1;
        in.runHeld = !settled;
        if (t < plan.waitTicks) {
            in.move = 0;
            in.runHeld = false;
        } else if (running < plan.jumpTicks) {
            in.jumpHeld = true;
            in.jumpPressed = !held;
        }
        held = in.jumpHeld;
        trial.tick(in);
        (void)trial.events().take();
        switch (trial.phase()) {
        case Phase::Dying: return -1e6f + static_cast<float>(t);
        case Phase::Goal:
        case Phase::BossDown: return 1e6f - static_cast<float>(t);
        case Phase::Over: return trial.outcome() == Outcome::Died ? -1e6f + static_cast<float>(t) : 1e6f;
        default: break;
        }
        if (trial.hero().power < session.hero().power) {
            return -5e5f + static_cast<float>(t); // a hit: almost as bad
        }
    }
    // Staying on the ground counts for a little: no hopping along flat ground.
    const auto& body = trial.hero().body;
    if (!body.grounded && body.velocity.y > 0.0f) {
        return -2e5f - body.box.y; // still falling after all this time: into a pit
    }
    return body.box.x + (body.grounded ? 8.0f : 0.0f) + (plan.jumpTicks == 0 ? 40.0f : 0.0f);
}

Input DemoBot::next(const Session& session)
{
    Input input;
    if (session.phase() != Phase::Playing) {
        m_held = false;
        m_hold = 0;
        m_wait = 0;
        return input;
    }
    const gamelib::core::PlatformerBody& body = session.hero().body;
    input.move = 1;
    input.runHeld = true;

    // Progress, or being stuck (a wall the plans cannot see past).
    if (body.box.x > m_bestX + 1.0f) {
        m_bestX = body.box.x;
        m_stuckTicks = 0;
    } else if (m_wait == 0) {
        ++m_stuckTicks;
    }
    if (m_backoff > 0) {
        --m_backoff;
        input.move = -1;
        m_held = false;
        return input;
    }
    if (m_stuckTicks > 120) {
        m_stuckTicks = 0;
        m_bestX = body.box.x;
        m_backoff = 30;
        return input;
    }

    // On the ground, look ahead every few ticks and pick the best plan.
    if (body.grounded && m_hold == 0 && --m_think <= 0) {
        m_think = kThinkEvery;
        Plan best;
        float bestScore = -std::numeric_limits<float>::infinity();
        for (const int jump : kJumps) {
            const Plan plan{.jumpTicks = jump};
            const float score = rate(session, plan);
            if (score > bestScore + 1.0f) {
                bestScore = score;
                best = plan;
            }
        }
        if (bestScore < 0.0f) {
            // Nothing works from here yet: wait a little and look again (a
            // platform coming round, a plant sinking), unless waiting fails too.
            best = Plan{.jumpTicks = 0, .waitTicks = 3};
            bestScore = 0.0f; // only a plan that survives beats waiting
            for (const int wait : kWaits) {
                for (const int jump : kJumps) {
                    const Plan plan{.jumpTicks = jump, .waitTicks = wait};
                    const float score = rate(session, plan);
                    if (score > bestScore + 1.0f) {
                        bestScore = score;
                        best = plan;
                    }
                }
            }
        }
        m_wait = best.waitTicks;
        m_hold = best.jumpTicks;

    }

    if (m_wait > 0) {
        --m_wait;
        input.move = 0;
        input.runHeld = false;
        m_held = false;
        return input;
    }
    if (m_hold > 0) {
        input.jumpHeld = true;
        input.jumpPressed = !m_held;
        --m_hold;
    }
    m_held = input.jumpHeld;
    // On fire: throw now and then.
    if (session.run().power == Power::Fire && session.ticks() % 40 == 0) {
        input.runPressed = true;
    }
    return input;
}

} // namespace bounder::game
