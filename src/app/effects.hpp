#pragma once

// Presentation-only juice, on real frame time: dust on jumps, landings and
// skids, brick shards, coins popping out of blocks, rings and sparkles for
// power-ups, stomp stars, score popups, the flash and the screen shake, and
// the hero's squash and stretch. Everything is in world units (drawn under
// the level's view) except the flash and the shake (canvas units).

#include "session.hpp"

#include <gamelib/core/camera.hpp>
#include <gamelib/core/color.hpp>
#include <gamelib/core/math.hpp>
#include <gamelib/core/rng.hpp>
#include <gamelib/draw/draw_list.hpp>
#include <gamelib/fx/juice.hpp>
#include <gamelib/fx/particles.hpp>
#include <gamelib/fx/sprite_motion.hpp>
#include <gamelib/fx/transients.hpp>

#include <string>

namespace bounder::app {

struct Shapes;

class Effects {
public:
    Effects();

    // Reacts to one simulation event.
    void onEvent(const game::Event& event, game::Theme theme);
    void update(float seconds);
    void clear();
    // Fireworks for the ending (world units).
    void firework(gamelib::core::Vec2 at, gamelib::core::Color color);

    // World layer (under the level's view).
    void drawWorld(gamelib::draw::DrawList& list, const Shapes& shapes) const;
    // Score popups, in canvas units through the view.
    void drawPopups(gamelib::draw::DrawList& list, const gamelib::core::View2D& view) const;
    void drawFlash(gamelib::draw::DrawList& list, gamelib::core::Size canvas) const;

    [[nodiscard]] gamelib::core::Vec2 shake(float time) const { return m_shake.offset(time); }
    [[nodiscard]] gamelib::core::Vec2 heroScale() const { return m_squash.scale(); }

private:
    struct Puff {
        gamelib::core::Vec2 at;
        gamelib::core::Vec2 drift;
        float size = 3.0f;
        gamelib::core::Color color;
    };
    struct Ring {
        gamelib::core::Vec2 at;
        float radius = 16.0f;
        gamelib::core::Color color;
    };
    struct CoinPop {
        gamelib::core::Vec2 at;
    };
    struct Popup {
        gamelib::core::Vec2 at;
        std::string text;
        gamelib::core::Color color;
    };
    struct Spark {
        gamelib::core::Vec2 at;
        float angle = 0.0f;
        gamelib::core::Color color;
    };

    void puffs(gamelib::core::Vec2 at, int count, float spread, gamelib::core::Vec2 drift, gamelib::core::Color color);
    void sparks(gamelib::core::Vec2 at, int count, gamelib::core::Color color);

    gamelib::fx::Particles m_debris;
    gamelib::fx::Transients<Puff> m_puffs;
    gamelib::fx::Transients<Ring> m_rings;
    gamelib::fx::Transients<CoinPop> m_coins;
    gamelib::fx::Transients<Popup> m_popups;
    gamelib::fx::Transients<Spark> m_sparks;
    gamelib::fx::Fade m_flash{0.25f};
    gamelib::fx::Shake m_shake{0.35f, 16.0f};
    gamelib::fx::SquashStretch m_squash{6.0f, 0.4f};
    gamelib::core::Rng m_rng;
};

} // namespace bounder::app
