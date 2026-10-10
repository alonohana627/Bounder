#include "demo_bot.hpp"

#include <gamelib/core/collision.hpp>

#include <algorithm>
#include <cmath>

namespace bounder::game {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Rect;
using gamelib::core::TileShape;

bool _standable(const TileShape shape)
{
    return shape == TileShape::Solid || shape == TileShape::OneWay || gamelib::core::isRamp(shape);
}

int _tile(const float coordinate)
{
    return static_cast<int>(std::floor(coordinate / kTile));
}

// Is there something to land on at x, from the feet down to a few rows below?
bool _groundAt(const Session& session, const gamelib::core::TileWorld& world, const float x, const float feet)
{
    const int column = _tile(x);
    for (int row = _tile(feet - 1.0f); row <= _tile(feet) + 4; ++row) {
        const TileShape shape = world.shapeAt({column, row});
        if (_standable(shape)) {
            return true;
        }
        if (shape == TileShape::Hazard) {
            return false;
        }
    }
    for (const Platform& p : session.area().platforms) {
        if (x >= p.box.left() && x <= p.box.right() && p.box.top() > feet - 3.0f * kTile && p.box.top() < feet + 5.0f * kTile) {
            return true;
        }
    }
    return false;
}

} // namespace

Input DemoBot::next(const Session& session)
{
    Input input;
    if (session.phase() != Phase::Playing) {
        m_held = false;
        m_hold = 0;
        return input;
    }
    const gamelib::core::PlatformerBody& body = session.hero().body;
    const Rect& box = body.box;
    const gamelib::core::TileWorld world = session.tileWorld();
    input.move = 1;
    input.runHeld = true;

    // Progress, or being stuck.
    if (box.x > m_bestX + 1.0f) {
        m_bestX = box.x;
        m_stuckTicks = 0;
    } else {
        ++m_stuckTicks;
    }
    if (m_backoff > 0) {
        --m_backoff;
        input.move = -1;
        input.runHeld = false;
        m_held = false;
        return input;
    }
    if (m_stuckTicks > 50) {
        m_stuckTicks = 0;
        m_bestX = box.x;
        if (++m_retries > 2) {
            m_retries = 0;
            m_backoff = 24;
            return input;
        }
        m_hold = 26;
    }

    bool jump = m_hold > 0;
    if (body.grounded && m_hold == 0) {
        const float ahead = box.right();
        // A wall ahead: jump, longer for a taller one.
        int wall = 0;
        const int column = _tile(ahead + 6.0f);
        for (int row = _tile(box.bottom() - 1.0f); row >= _tile(box.bottom() - 1.0f) - 4; --row) {
            if (world.shapeAt({column, row}) != TileShape::Solid) {
                break;
            }
            ++wall;
        }
        if (wall > 0) {
            m_hold = 8 + 5 * wall;
        }
        // A gap or spikes ahead: jump at its edge, holding for the whole arc.
        if (m_hold == 0 && _groundAt(session, world, box.center().x, box.bottom())) {
            for (float d = 4.0f; d <= 14.0f; d += 5.0f) {
                if (!_groundAt(session, world, ahead + d, box.bottom())) {
                    m_hold = 24;
                    break;
                }
            }
        }
        // An enemy ahead at the hero's height: jump onto it or over it.
        for (const auto [handle, enemy] : session.area().enemies) {
            if (!enemy.awake || enemy.state == EnemyState::Knocked || enemy.state == EnemyState::Squashed) {
                continue;
            }
            const float gap = enemy.box.left() - ahead;
            const bool level = enemy.box.bottom() > box.top() - 10.0f && enemy.box.top() < box.bottom() + 6.0f;
            if (enemy.kind == EnemyKind::Plant) {
                const bool hidden = enemy.box.bottom() >= enemy.origin.y + enemy.box.height / 2.0f - 1.0f;
                if (!hidden && gap > -8.0f && gap < 3.0f * kTile) {
                    input.move = 0; // wait for it to sink
                    m_hold = 0;
                    break;
                }
                continue;
            }
            if (enemy.state == EnemyState::ShellIdle) {
                continue; // walking into it kicks it away
            }
            if (level && gap > -4.0f && gap < 2.4f * kTile + std::abs(body.velocity.x) * 4.0f) {
                m_hold = std::max(m_hold, enemy.kind == EnemyKind::Spiky ? 20 : 12);
            }
        }
        jump = m_hold > 0;
    }
    if (jump) {
        input.jumpHeld = true;
        input.jumpPressed = !m_held;
        m_hold = std::max(0, m_hold - 1);
    }
    m_held = input.jumpHeld;
    // On fire: throw at enemies now and then.
    if (session.run().power == Power::Fire && session.ticks() % 40 == 0) {
        input.runPressed = true;
    }
    return input;
}

} // namespace bounder::game
