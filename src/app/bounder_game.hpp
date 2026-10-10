#pragma once

// Bounder on GameLib's main loop: the title (the demo bot playing world 1
// behind the menu), the overworld maps (the level select), the levels with
// an iris in and a card with the lives left, pause, the course-clear and
// game-over screens, and the ending after the castle. The rules live in
// src/game; this is input, presentation, sound and saves.

#include "effects.hpp"
#include "map_view.hpp"
#include "profile.hpp"
#include "session.hpp"
#include "sounds.hpp"
#include "view.hpp"

#include "demo_bot.hpp"

#include <gamelib/ai/attract.hpp>
#include <gamelib/app/app.hpp>
#include <gamelib/app/arcade_menus.hpp>
#include <gamelib/audio/sound_bank.hpp>
#include <gamelib/fx/debug_overlay.hpp>
#include <gamelib/fx/transients.hpp>
#include <gamelib/ui/transition.hpp>

#include <optional>
#include <string>
#include <vector>

namespace bounder::app {

class BounderGame final : public gamelib::app::Game {
public:
    // demo: the demo plays full screen without the menu (--demo).
    BounderGame(gamelib::app::App& app, ProfileFile profile, bool demo);

    void onFocus(gamelib::app::App& app, bool focused) override;
    void onKey(gamelib::app::App& app, const gamelib::platform::KeyEvent& event) override;
    void onGamepad(gamelib::app::App& app, const gamelib::platform::GamepadEvent& event) override;
    void tick(gamelib::app::App& app) override;
    void frame(gamelib::app::App& app, const gamelib::app::FrameInfo& info) override;
    void draw(gamelib::app::App& app, gamelib::draw::DrawList& list, const gamelib::app::FrameInfo& info) override;

private:
    enum class Mode { Map, Level, Ending };
    enum class MapCommand { None, Up, Down, Left, Right, Confirm };

    // The demo: the bot playing world 1's levels one after another.
    struct DemoRun {
        game::Session session;
        game::DemoBot bot;
        int level = 0;
        int attempts = 0;
        int generation = 0; // bumped on every new attempt, so the view snaps
    };
    struct Banner {
        std::string title;
        std::string line;
        gamelib::core::Color color;
    };
    // The card shown while a transition covers the screen.
    struct Card {
        std::string title;
        std::string line;
        int lives = 0;
    };

    void handleResult(gamelib::app::App& app, const gamelib::app::ArcadeResult& result, gamelib::app::ArcadeScreen before);
    void mapCommand(gamelib::app::App& app, MapCommand command);
    void enterMap(gamelib::app::App& app);
    void startLevel(gamelib::app::App& app, int level, const game::Carry& carry, bool iris);
    void leaveLevel(gamelib::app::App& app);
    [[nodiscard]] game::Input readInput(const gamelib::platform::InputState& input) const;
    void mapTick();
    void levelTick(gamelib::app::App& app);
    void demoTick();
    void react(gamelib::app::App& app, const game::Event& event);
    void attemptOver(gamelib::app::App& app);
    void updateMusic(gamelib::app::App& app);
    void save();
    [[nodiscard]] bool showsLevel() const;
    [[nodiscard]] game::IsCleared clearedTest() const;
    [[nodiscard]] game::Carry freshCarry() const;

    void drawHud(gamelib::draw::DrawList& list, gamelib::core::Size canvas, const game::Session& session) const;
    void drawCard(gamelib::draw::DrawList& list, gamelib::core::Size canvas) const;
    void drawBanners(gamelib::draw::DrawList& list, gamelib::core::Size canvas) const;
    void drawEnding(gamelib::draw::DrawList& list, gamelib::core::Size canvas) const;
    void drawMenus(gamelib::draw::DrawList& list, gamelib::core::Size canvas) const;
    void drawDebug(gamelib::draw::DrawList& list, const game::Session& session, const LevelView& view);

    ProfileFile m_profile; // the menus edit its settings in place: declared first
    gamelib::app::ArcadeMenus m_menus;
    gamelib::audio::SoundBank<Sfx, kSfxCount> m_sfx;
    Music m_music;
    Shapes m_shapes;
    gamelib::core::Size m_canvas;
    Mode m_mode = Mode::Map;

    // The overworld: the hero's node, or the cells he walks to the next one.
    int m_node = 0;
    std::vector<gamelib::core::Cell> m_route;
    std::size_t m_routeStep = 0;
    float m_routeT = 0.0f;
    gamelib::core::Vec2 m_mapHero;
    int m_mapFacing = 1;
    float m_mapWalked = 0.0f;

    // The level being played.
    std::optional<game::Session> m_session;
    int m_level = 0;
    game::RunState m_levelStartRun; // to restart a level or leave it without losing anything
    std::uint8_t m_foundBefore = 0; // star coins of this level found on earlier visits
    LevelView m_view;
    Effects m_effects;
    gamelib::ui::Transition m_transition;
    std::optional<Card> m_card;
    float m_clearTimer = -1.0f;   // seconds until the course-clear returns to the map
    bool m_gameOver = false;
    bool m_starMusic = false;
    int m_startLevel = -1;        // BOUNDER_START_LEVEL: PLAY goes straight into a level

    // The demo behind the title.
    gamelib::ai::AttractMode<DemoRun> m_attract;
    LevelView m_demoView;
    Effects m_demoEffects;
    int m_demoGeneration = -1;
    bool m_demoMode = false;

    gamelib::fx::Transients<Banner> m_banners;
    gamelib::fx::DebugOverlay m_debug;
    float m_time = 0.0f;
    float m_endingTime = 0.0f;
    float m_nextFirework = 0.0f;
    bool m_newBest = false;
    std::int64_t m_endingScore = 0;
};

} // namespace bounder::app
