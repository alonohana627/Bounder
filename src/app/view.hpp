#pragma once

// Drawing a level: the camera (GameLib's Camera2D following the hero with a
// dead zone and lookahead, held in the level's bounds or a boss arena), the
// parallax backdrop (sky, far hills, clouds, near bushes), the tiles (one
// tile shader for every terrain and block kind; bumped blocks hop), the
// furniture (goal pole, castle, checkpoint, platforms), coins, star coins,
// items, enemies, fireballs and the hero. Presentation only.

#include "effects.hpp"
#include "session.hpp"

#include <gamelib/core/camera.hpp>
#include <gamelib/draw/draw_list.hpp>
#include <gamelib/draw/tile_map.hpp>
#include <gamelib/gfx/renderer.hpp>

#include <cstdint>

namespace bounder::app {

// The game's own shader shapes, registered once.
struct Shapes {
    gamelib::draw::ShapeId tile;
    gamelib::draw::ShapeId hero;
    gamelib::draw::ShapeId enemy;
    gamelib::draw::ShapeId item;
    gamelib::draw::ShapeId scenery;
    gamelib::draw::ShapeId sky;
    gamelib::draw::ShapeId bushes;
    gamelib::draw::ShapeId map;
    gamelib::draw::ShapeId far;

    [[nodiscard]] static Shapes registerWith(gamelib::gfx::Renderer& renderer);
};

inline constexpr float kZoom = 4.5f; // canvas units per world unit: 15 tiles fill the 1080 canvas rows

struct LevelDrawOptions {
    float alpha = 1.0f;         // interpolation between the last two ticks
    gamelib::core::Vec2 shake;  // canvas units
    std::uint8_t foundBefore = 0; // star coins found on earlier visits (drawn as ghosts until taken)
};

class LevelView {
public:
    explicit LevelView(gamelib::core::Size canvas);

    // Jumps the camera to the hero (a new attempt, a new area).
    void snap(const game::Session& session);
    // One frame of camera following (call before draw).
    void follow(const game::Session& session, float alpha, float seconds);
    void draw(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, const Effects& effects,
              const LevelDrawOptions& options) const;
    // The level's view this frame (the effects' popups use it).
    [[nodiscard]] gamelib::core::View2D view(gamelib::core::Vec2 shake = {}) const { return m_camera.view(shake); }

private:
    void drawBackdrop(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const;
    void drawTiles(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const;
    void drawFurniture(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, float alpha,
                       std::uint8_t foundBefore) const;
    void drawActors(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                    const Effects& effects, float alpha) const;
    void drawHero(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                  const Effects& effects, float alpha) const;

    gamelib::core::Size m_canvas;
    gamelib::core::Camera2D m_camera;
    int m_area = -1;
    bool m_locked = false;
};

} // namespace bounder::app
