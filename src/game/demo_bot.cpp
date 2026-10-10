#include "demo_bot.hpp"

#include <array>

namespace bounder::game {

namespace {

// helpers

constexpr int kHorizon = 90;   // ticks each plan is tried for
constexpr int kThinkEvery = 2; // ticks between looks ahead on the ground

using Plan = DemoBot::Plan;
constexpr std::array<int, 7> kJumps{0, 4, 8, 12, 16, 20, 26};
constexpr std::array<int, 2> kWaits{14, 36};

// Go now, holding jump for 0 to 26 ticks.
constexpr std::array<Plan, kJumps.size()> kGoPlans = [] {
    std::array<Plan, kJumps.size()> plans{};
    for (std::size_t j = 0; j < kJumps.size(); ++j) {
        plans[j] = {.jumpTicks = kJumps[j], .waitTicks = 0};
    }
    return plans;
}();
// Wait first, then go.
constexpr std::array<Plan, kJumps.size() * kWaits.size()> kWaitPlans = [] {
    std::array<Plan, kJumps.size() * kWaits.size()> plans{};
    for (std::size_t w = 0; w < kWaits.size(); ++w) {
        for (std::size_t j = 0; j < kJumps.size(); ++j) {
            plans[w * kJumps.size() + j] = {.jumpTicks = kJumps[j], .waitTicks = kWaits[w]};
        }
    }
    return plans;
}();


} // namespace

std::optional<gamelib::ai::PlanChoice> DemoBot::choose(const Session& session, const std::span<const Plan> plans) const
{
    // A plan: wait, then run right (jumping as planned) until the hero
    // lands again - or, without a jump, for half a second - then let go and
    // see where he comes to rest. Running on blindly would count every
    // ledge after the landing against the jump.
    struct Trial {
        Session session;
        Power power;
        bool held = false;
        bool airborne = false;
        bool settled = false;
    };
    const gamelib::ai::PlanRules<Trial, Plan> rules{
        .step = [](Trial& trial, const Plan& plan, const int t) -> std::optional<float> {
            Input in;
            const int running = t - plan.waitTicks;
            if (!trial.session.hero().body.grounded) {
                trial.airborne = true;
            } else if (trial.airborne || (plan.jumpTicks == 0 && running >= 30)) {
                trial.settled = true;
            }
            in.move = trial.settled ? 0 : 1;
            in.runHeld = !trial.settled;
            if (t < plan.waitTicks) {
                in.move = 0;
                in.runHeld = false;
            } else if (running < plan.jumpTicks) {
                in.jumpHeld = true;
                in.jumpPressed = !trial.held;
            }
            trial.held = in.jumpHeld;
            trial.session.tick(in);
            (void)trial.session.events().take();
            const float tick = static_cast<float>(t);
            switch (trial.session.phase()) {
            case Phase::Dying: return -1e6f + tick;
            case Phase::Goal:
            case Phase::BossDown: return 1e6f - tick;
            case Phase::Over: return trial.session.outcome() == Outcome::Died ? -1e6f + tick : 1e6f;
            default: break;
            }
            if (trial.session.hero().power < trial.power) {
                return -5e5f + tick; // a hit: almost as bad
            }
            return std::nullopt;
        },
        .rate = [](const Trial& trial, const Plan& plan) {
            const auto& body = trial.session.hero().body;
            if (!body.grounded && body.velocity.y > 0.0f) {
                return -2e5f - body.box.y; // still falling after all this time: into a pit
            }
            // Staying on the ground counts for a little: no hopping along flat ground.
            return body.box.x + (body.grounded ? 8.0f : 0.0f) + (plan.jumpTicks == 0 ? 40.0f : 0.0f);
        }};
    return gamelib::ai::bestPlan(Trial{.session = session, .power = session.hero().power, .held = m_held}, plans, rules,
                                 kHorizon, 1.0f);
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
        const std::optional<gamelib::ai::PlanChoice> now = choose(session, kGoPlans);
        Plan best = kGoPlans[now->index];
        if (now->rating < 0.0f) {
            // Nothing works from here yet: wait a little and look again (a
            // platform coming round, a plant sinking), unless waiting fails
            // too - only a plan that survives beats waiting.
            const std::optional<gamelib::ai::PlanChoice> later = choose(session, kWaitPlans);
            best = later->rating > 1.0f ? kWaitPlans[later->index] : Plan{.jumpTicks = 0, .waitTicks = 3};
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
