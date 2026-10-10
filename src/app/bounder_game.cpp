#include "bounder_game.hpp"

#include "palette.hpp"

#include <gamelib/app/settings.hpp>
#include <gamelib/core/easing.hpp>
#include <gamelib/draw/vector_text.hpp>
#include <gamelib/ui/menu.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace bounder::app {

namespace {

// helpers

using gamelib::app::ArcadeAction;
using gamelib::app::ArcadeScreen;
using gamelib::core::Color;
using gamelib::core::Rect;
using gamelib::core::Vec2;
using gamelib::platform::GamepadButton;
using gamelib::platform::Key;

constexpr float kMapCellsPerSecond = 5.0f;
constexpr float kClearSeconds = 2.6f;
constexpr float kEndingMenuDelay = 9.0f;
constexpr int kDemoLevels = 4; // world 1's four levels
constexpr int kExitToMap = 0; // the pause menu's EXIT TO MAP

gamelib::ui::MenuStyle _menuStyle()
{
    return {.veil = Color{0.02f, 0.02f, 0.06f, 0.45f},
            .panel = srgb(0x1A1430, 0.98f),
            .border = srgb(0xFFC93C),
            .title = srgb(0xFFC93C),
            .text = kText,
            .highlight = srgb(0xFFE14D),
            .selection = srgb(0x3A2E6A),
            .dim = kTextDim,
            .titlePixel = 13.0f,
            .itemPixel = 6.0f,
            .glowWidth = 22.0f};
}

game::Session _demoSession(const int level)
{
    game::Carry carry;
    carry.run.lives = 9;
    return game::Session{level, carry};
}

Tune _tuneFor(const game::Theme theme)
{
    switch (theme) {
    case game::Theme::Grass: return Tune::Grass;
    case game::Theme::Desert: return Tune::Desert;
    case game::Theme::Ice: return Tune::Ice;
    case game::Theme::Sky: return Tune::Sky;
    case game::Theme::Fortress:
    case game::Theme::Castle: return Tune::Fortress;
    case game::Theme::Bonus: return Tune::Bonus;
    }
    return Tune::Grass;
}

std::string _cardTitle(const int level)
{
    const game::LevelDef& def = game::level(level);
    return level == game::kCastleLevel ? std::string{"THE ROYAL CASTLE"}
                                       : "WORLD " + std::string{def.id} + "   " + std::string{def.name};
}

} // namespace

BounderGame::BounderGame(gamelib::app::App& app, ProfileFile profile, const bool demo)
    : m_profile{std::move(profile)}
    , m_menus{"BOUNDER", m_profile.value().settings, {}, {.music = true, .vsync = true},
              [](gamelib::ui::Menu& paused) { paused.action("EXIT TO MAP", kExitToMap); }}
    , m_sfx{app.audio(), renderSfx}
    , m_music{app.audio(), 2, tuneTake}
    , m_shapes{Shapes::registerWith(app.renderer())}
    , m_canvas{app.canvas()}
    , m_view{app.canvas()}
    , m_dev{gamelib::app::readDevStart("BOUNDER")}
    , m_startLevel{m_dev.level && *m_dev.level < game::kLevelCount ? *m_dev.level : -1}
    , m_attract{[](std::uint64_t) { return DemoRun{.session = _demoSession(0), .bot = {}}; },
                [](DemoRun& run, float) {
                    run.session.tick(run.bot.next(run.session));
                    if (run.session.phase() != game::Phase::Over) {
                        return;
                    }
                    // Cleared: on to the next level; died: try again from the checkpoint.
                    game::Carry carry = run.session.carry();
                    carry.run.lives = 9;
                    if (run.session.outcome() == game::Outcome::Cleared) {
                        ++run.level;
                        carry = {};
                        carry.run = run.session.run();
                        carry.run.lives = 9;
                        run.attempts = 0;
                    } else {
                        ++run.attempts;
                    }
                    run.session = game::Session{std::min(run.level, kDemoLevels - 1), carry};
                    run.bot = {};
                    ++run.generation;
                },
                [](const DemoRun& run) { return run.level >= kDemoLevels || run.attempts >= 5; },
                1}
    , m_demoView{app.canvas()}
    , m_demoMode{demo}
{
    Progress& progress = m_profile.value().game;
    progress.world = std::clamp(progress.world, 0, game::kWorlds - 1);
    const game::WorldMap& map = game::worldMap(progress.world);
    progress.node = std::clamp(progress.node, 0, static_cast<int>(map.nodes.size()) - 1);
    m_node = progress.node;
    m_mapHero = {static_cast<float>(map.nodes[static_cast<std::size_t>(m_node)].cell.x),
                 static_cast<float>(map.nodes[static_cast<std::size_t>(m_node)].cell.y)};
    m_profile.save(); // a missing file is written with the defaults, so it can be found and edited
    gamelib::app::applySettings(app, m_profile.value().settings);
    m_music.play(app.audio(), Tune::Title, 0.6f);
}

void BounderGame::save()
{
    m_profile.value().game.node = m_node;
    m_profile.save();
}

game::IsCleared BounderGame::clearedTest() const
{
    return [this](const int level) { return m_profile.value().game.campaign.record(levelId(level)).cleared; };
}

game::Carry BounderGame::freshCarry() const
{
    return game::Carry{.run = m_profile.value().game.run};
}

void BounderGame::onFocus(gamelib::app::App& app, const bool focused)
{
    if (!m_demoMode) {
        m_menus.onFocus(app, focused, m_time);
    }
}

void BounderGame::onKey(gamelib::app::App& app, const gamelib::platform::KeyEvent& event)
{
    if (event.pressed && event.key == Key::F3) {
        m_debug.toggle();
        return;
    }
    if (m_demoMode) {
        if (event.pressed && event.key != Key::F11) {
            m_demoMode = false; // any key leaves the full-screen demo for the title
            app.input().releaseAll();
        }
        return;
    }
    const gamelib::app::ArcadeResult result = m_menus.onKey(app, event, m_time);
    handleResult(app, result);
    if (!event.pressed || result.from != ArcadeScreen::Playing || !m_menus.is(ArcadeScreen::Playing) || m_mode != Mode::Map) {
        return;
    }
    switch (event.key) {
    case Key::Up:
    case Key::W: mapCommand(app, MapCommand::Up); break;
    case Key::Down:
    case Key::S: mapCommand(app, MapCommand::Down); break;
    case Key::Left:
    case Key::A: mapCommand(app, MapCommand::Left); break;
    case Key::Right:
    case Key::D: mapCommand(app, MapCommand::Right); break;
    case Key::Z:
    case Key::Space:
    case Key::Enter:
    case Key::K:
    case Key::C: mapCommand(app, MapCommand::Confirm); break;
    default: break;
    }
}

void BounderGame::onGamepad(gamelib::app::App& app, const gamelib::platform::GamepadEvent& event)
{
    if (m_demoMode) {
        if (event.pressed) {
            m_demoMode = false;
            app.input().releaseAll();
        }
        return;
    }
    const gamelib::app::ArcadeResult result = m_menus.onGamepad(app, event, m_time);
    handleResult(app, result);
    if (!event.pressed || result.from != ArcadeScreen::Playing || !m_menus.is(ArcadeScreen::Playing) || m_mode != Mode::Map) {
        return;
    }
    switch (event.button) {
    case GamepadButton::DpadUp:
    case GamepadButton::LeftStickUp: mapCommand(app, MapCommand::Up); break;
    case GamepadButton::DpadDown:
    case GamepadButton::LeftStickDown: mapCommand(app, MapCommand::Down); break;
    case GamepadButton::DpadLeft:
    case GamepadButton::LeftStickLeft: mapCommand(app, MapCommand::Left); break;
    case GamepadButton::DpadRight:
    case GamepadButton::LeftStickRight: mapCommand(app, MapCommand::Right); break;
    case GamepadButton::A: mapCommand(app, MapCommand::Confirm); break;
    default: break;
    }
}

void BounderGame::handleResult(gamelib::app::App& app, const gamelib::app::ArcadeResult& result)
{
    if (result.changed) {
        m_profile.save();
    }
    switch (result.action) {
    case ArcadeAction::Start:
        if (result.from == ArcadeScreen::Paused) {
            // RESTART: the level from its start, as it was entered.
            if (m_mode == Mode::Level && m_session) {
                startLevel(app, m_level, game::Carry{.run = m_levelStartRun}, false);
            }
            break;
        }
        if (result.from == ArcadeScreen::GameOver) {
            // PLAY AGAIN after a game over or the ending: a fresh run, progress kept.
            m_profile.value().game.run = {};
        }
        enterMap(app);
        if (m_startLevel >= 0 && result.from == ArcadeScreen::Title) {
            startLevel(app, m_startLevel, freshCarry(), true);
        }
        break;
    case ArcadeAction::ToTitle:
        if (m_mode == Mode::Level && m_session && !m_gameOver) {
            m_profile.value().game.run = m_levelStartRun; // leaving costs nothing
        }
        m_session.reset();
        m_transition.cancel();
        m_card.reset();
        m_mode = Mode::Map;
        m_gameOver = false;
        save();
        m_music.play(app.audio(), Tune::Title, 0.6f);
        break;
    case ArcadeAction::Quit:
        if (m_mode == Mode::Level && m_session && !m_gameOver) {
            m_profile.value().game.run = m_levelStartRun;
        }
        save();
        app.requestQuit();
        break;
    case ArcadeAction::Game:
        // EXIT TO MAP: back to the level's node; leaving costs nothing.
        if (result.command == kExitToMap && m_mode == Mode::Level && m_session && !m_gameOver && m_clearTimer < 0.0f
            && !m_transition.active()) {
            m_profile.value().game.run = m_levelStartRun;
            m_music.stop(app.audio(), 0.3f);
            leaveLevel(app);
        }
        break;
    case ArcadeAction::Resume:
    case ArcadeAction::Controls:
    case ArcadeAction::None: break;
    }
    switch (result.feedback) {
    case gamelib::ui::MenuFeedback::Selected: m_sfx.queue(app.sounds(), Sfx::MenuSelect); break;
    case gamelib::ui::MenuFeedback::Back: m_sfx.queue(app.sounds(), Sfx::MenuBack); break;
    case gamelib::ui::MenuFeedback::Moved:
    case gamelib::ui::MenuFeedback::Changed: m_sfx.queue(app.sounds(), Sfx::MenuMove); break;
    case gamelib::ui::MenuFeedback::None: break;
    }
}

void BounderGame::enterMap(gamelib::app::App& app)
{
    Progress& progress = m_profile.value().game;
    if (progress.run.lives <= 0) {
        progress.run = {};
    }
    m_mode = Mode::Map;
    m_session.reset();
    m_gameOver = false;
    m_card.reset();
    m_route.clear();
    const game::WorldMap& map = game::worldMap(progress.world);
    m_node = std::clamp(m_node, 0, static_cast<int>(map.nodes.size()) - 1);
    const gamelib::core::Cell cell = map.nodes[static_cast<std::size_t>(m_node)].cell;
    m_mapHero = {static_cast<float>(cell.x), static_cast<float>(cell.y)};
    save();
    m_music.play(app.audio(), Tune::Map, 0.6f);
}

void BounderGame::mapCommand(gamelib::app::App& app, const MapCommand command)
{
    if (!m_route.empty() || m_transition.active() || command == MapCommand::None) {
        return;
    }
    Progress& progress = m_profile.value().game;
    const game::WorldMap& map = game::worldMap(progress.world);
    const game::MapNode& node = map.nodes[static_cast<std::size_t>(m_node)];
    if (command == MapCommand::Confirm) {
        if (node.kind == game::NodeKind::Level) {
            m_sfx.queue(app.sounds(), Sfx::MapEnter);
            startLevel(app, node.level, freshCarry(), true);
        } else if (node.kind == game::NodeKind::NextWorld || (node.kind == game::NodeKind::PrevWorld && progress.world > 0)) {
            const bool next = node.kind == game::NodeKind::NextWorld;
            if (next && !game::nodeCleared(node, progress.world, clearedTest())) {
                return;
            }
            m_sfx.queue(app.sounds(), Sfx::Pipe);
            m_transition.start({.kind = gamelib::ui::TransitionKind::Fade, .color = kInk, .coverSeconds = 0.35f,
                                .holdSeconds = 0.15f, .uncoverSeconds = 0.35f},
                               [this, next] {
                                   Progress& p = m_profile.value().game;
                                   p.world += next ? 1 : -1;
                                   const game::WorldMap& to = game::worldMap(p.world);
                                   m_node = to.start;
                                   if (!next) {
                                       for (std::size_t i = 0; i < to.nodes.size(); ++i) {
                                           if (to.nodes[i].kind == game::NodeKind::NextWorld) {
                                               m_node = static_cast<int>(i);
                                           }
                                       }
                                   }
                                   const gamelib::core::Cell c = to.nodes[static_cast<std::size_t>(m_node)].cell;
                                   m_mapHero = {static_cast<float>(c.x), static_cast<float>(c.y)};
                                   save();
                               });
        }
        return;
    }
    gamelib::core::Direction direction = gamelib::core::Direction::Up;
    switch (command) {
    case MapCommand::Down: direction = gamelib::core::Direction::Down; break;
    case MapCommand::Left: direction = gamelib::core::Direction::Left; break;
    case MapCommand::Right: direction = gamelib::core::Direction::Right; break;
    default: break;
    }
    if (std::optional<std::vector<gamelib::core::Cell>> route = game::route(map, m_node, direction, clearedTest())) {
        m_route = std::move(*route);
        m_routeStep = 0;
        m_routeT = 0.0f;
        m_sfx.queue(app.sounds(), Sfx::MapMove);
    }
}

void BounderGame::mapTick()
{
    if (m_route.empty()) {
        return;
    }
    const game::WorldMap& map = game::worldMap(m_profile.value().game.world);
    m_routeT += kMapCellsPerSecond / static_cast<float>(game::kTickRate);
    m_mapWalked += kMapCellsPerSecond / static_cast<float>(game::kTickRate);
    while (m_routeT >= 1.0f && !m_route.empty()) {
        m_routeT -= 1.0f;
        ++m_routeStep;
        if (m_routeStep >= m_route.size()) {
            m_node = game::nodeAt(map, m_route.back());
            m_route.clear();
            m_routeT = 0.0f;
            const gamelib::core::Cell c = map.nodes[static_cast<std::size_t>(m_node)].cell;
            m_mapHero = {static_cast<float>(c.x), static_cast<float>(c.y)};
            save();
            return;
        }
    }
    const gamelib::core::Cell from = m_routeStep == 0 ? map.nodes[static_cast<std::size_t>(m_node)].cell : m_route[m_routeStep - 1];
    const gamelib::core::Cell to = m_route[m_routeStep];
    const Vec2 a{static_cast<float>(from.x), static_cast<float>(from.y)};
    const Vec2 b{static_cast<float>(to.x), static_cast<float>(to.y)};
    if (b.x != a.x) {
        m_mapFacing = b.x > a.x ? 1 : -1;
    }
    m_mapHero = gamelib::core::lerp(a, b, m_routeT);
}

void BounderGame::startLevel(gamelib::app::App& app, const int level, const game::Carry& carry, const bool iris)
{
    m_level = level;
    m_card = Card{.title = _cardTitle(level), .line = "", .lives = carry.run.lives};
    m_levelStartRun = carry.run;
    gamelib::ui::TransitionStyle style{.kind = iris ? gamelib::ui::TransitionKind::Iris : gamelib::ui::TransitionKind::Fade,
                                       .color = kInk, .coverSeconds = iris ? 0.5f : 0.35f, .holdSeconds = 1.4f,
                                       .uncoverSeconds = 0.45f};
    m_transition.start(style, [this, &app, level, carry] {
        m_mode = Mode::Level;
        m_session.emplace(level, carry);
        m_replay = level == m_startLevel ? m_dev.playback<game::Input>(game::decodeInput) : gamelib::core::Playback<game::Input>{};
        if (m_dev.column && level == m_startLevel) {
            m_session->debugStartAt(*m_dev.column);
        }
        m_foundBefore = static_cast<std::uint8_t>(m_profile.value().game.campaign.record(levelId(level)).found);
        m_view.snap(*m_session);
        m_effects.clear();
        m_clearTimer = -1.0f;
        m_gameOver = false;
        m_starMusic = false;
        m_music.stop(app.audio(), 0.3f);
    });
}

void BounderGame::leaveLevel(gamelib::app::App& app)
{
    m_transition.start({.kind = gamelib::ui::TransitionKind::Iris, .color = kInk, .coverSeconds = 0.5f, .holdSeconds = 0.2f,
                        .uncoverSeconds = 0.5f},
                       [this, &app] {
                           const int level = m_level;
                           enterMap(app);
                           // Stand on the level just played.
                           const game::WorldMap& map = game::worldMap(m_profile.value().game.world);
                           if (const int node = game::nodeOfLevel(map, level); node >= 0) {
                               m_node = node;
                               const gamelib::core::Cell c = map.nodes[static_cast<std::size_t>(node)].cell;
                               m_mapHero = {static_cast<float>(c.x), static_cast<float>(c.y)};
                           }
                           save();
                       });
}

game::Input BounderGame::readInput(const gamelib::platform::InputState& input) const
{
    game::Input in;
    const bool left = input.isAnyDown({Key::Left, Key::A}) || input.isDown(GamepadButton::DpadLeft)
                      || input.isDown(GamepadButton::LeftStickLeft);
    const bool right = input.isAnyDown({Key::Right, Key::D}) || input.isDown(GamepadButton::DpadRight)
                       || input.isDown(GamepadButton::LeftStickRight);
    in.move = (right ? 1 : 0) - (left ? 1 : 0);
    in.down = input.isAnyDown({Key::Down, Key::S}) || input.isDown(GamepadButton::DpadDown)
              || input.isDown(GamepadButton::LeftStickDown);
    in.jumpPressed = input.wasAnyPressed({Key::Z, Key::Space, Key::K, Key::C}) || input.wasPressed(GamepadButton::A);
    in.jumpHeld = in.jumpPressed || input.isAnyDown({Key::Z, Key::Space, Key::K, Key::C}) || input.isDown(GamepadButton::A);
    in.runPressed = input.wasAnyPressed({Key::X, Key::LeftShift, Key::RightShift, Key::J})
                    || input.wasPressed(GamepadButton::B) || input.wasPressed(GamepadButton::X);
    in.runHeld = in.runPressed || input.isAnyDown({Key::X, Key::LeftShift, Key::RightShift, Key::J})
                 || input.isDown(GamepadButton::B) || input.isDown(GamepadButton::X);
    return in;
}

void BounderGame::tick(gamelib::app::App& app)
{
    if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        demoTick();
    }
    if (!m_menus.is(ArcadeScreen::Playing) || m_demoMode) {
        return;
    }
    if (m_mode == Mode::Map) {
        mapTick();
    } else if (m_mode == Mode::Level) {
        levelTick(app);
    }
}

void BounderGame::levelTick(gamelib::app::App& app)
{
    if (!m_session || m_transition.active()) {
        return;
    }
    const std::optional<game::Input> replayed = m_replay.next();
    m_session->tick(replayed ? *replayed : readInput(app.input()));
    for (const game::Event& event : m_session->events().take()) {
        react(app, event);
    }
    if (m_session->phase() == game::Phase::Over && m_clearTimer < 0.0f && !m_gameOver) {
        attemptOver(app);
    }
}

void BounderGame::demoTick()
{
    m_attract.tick(1.0f / static_cast<float>(game::kTickRate));
    DemoRun& run = m_attract.sim();
    const int generation = run.generation + 1000 * m_attract.restarts();
    if (generation != m_demoGeneration) {
        m_demoGeneration = generation;
        m_demoView.snap(run.session);
        m_demoEffects.clear();
    }
    for (const game::Event& event : run.session.events().take()) {
        if (event.kind != game::EventKind::Points) {
            m_demoEffects.onEvent(event, run.session.area().theme);
        }
    }
}

void BounderGame::react(gamelib::app::App& app, const game::Event& event)
{
    using game::EventKind;
    m_effects.onEvent(event, m_session->area().theme);
    auto& sounds = app.sounds();
    const bool big = m_session->hero().power != game::Power::Small;
    switch (event.kind) {
    case EventKind::Jump: m_sfx.queue(sounds, big ? Sfx::BigJump : Sfx::Jump); break;
    case EventKind::Land: break;
    case EventKind::Skid: m_sfx.queue(sounds, Sfx::Skid); break;
    case EventKind::Stomp:
        m_sfx.queue(sounds, Sfx::Stomp, 1.0f, 1.0f + 0.06f * static_cast<float>(std::min(event.value, 8)));
        app.hitStop(0.035f);
        break;
    case EventKind::Kick: m_sfx.queue(sounds, Sfx::Kick); break;
    case EventKind::EnemyKnocked: m_sfx.queue(sounds, Sfx::Kick, 0.8f, 1.2f); break;
    case EventKind::Coin: m_sfx.queue(sounds, Sfx::Coin); break;
    case EventKind::BlockBump: m_sfx.queue(sounds, Sfx::Bump); break;
    case EventKind::BrickBreak: m_sfx.queue(sounds, Sfx::BrickBreak); break;
    case EventKind::ItemSpawn: m_sfx.queue(sounds, Sfx::Sprout); break;
    case EventKind::PowerUp:
        m_sfx.queue(sounds, Sfx::PowerUp);
        if (event.item == game::ItemKind::Star) {
            m_banners.add("STAR POWER!", "", kStar, 1.2f);
        }
        break;
    case EventKind::OneUp: m_sfx.queue(sounds, Sfx::OneUp); break;
    case EventKind::Hurt: m_sfx.queue(sounds, Sfx::Shrink); break;
    case EventKind::Fireball: m_sfx.queue(sounds, Sfx::Fireball, event.value == 1 ? 0.7f : 1.0f, event.value == 1 ? 0.7f : 1.0f); break;
    case EventKind::FireballPop: m_sfx.queue(sounds, Sfx::FireballPop); break;
    case EventKind::PipeIn:
    case EventKind::PipeOut: m_sfx.queue(sounds, Sfx::Pipe); break;
    case EventKind::Checkpoint: m_sfx.queue(sounds, Sfx::Checkpoint); break;
    case EventKind::StarCoin: m_sfx.queue(sounds, Sfx::StarCoin); break;
    case EventKind::GoalTouch:
        m_sfx.queue(sounds, Sfx::Pole);
        m_music.stop(app.audio(), 0.3f);
        break;
    case EventKind::Clear: {
        m_sfx.queue(sounds, Sfx::Clear);
        m_music.stop(app.audio(), 0.3f);
        // Record the clear: the best score and the star coins found.
        Progress& progress = m_profile.value().game;
        const gamelib::core::ClearResult result = progress.campaign.clear(
            levelId(m_level), {.score = m_session->levelPoints(), .found = m_session->starCoinMask()});
        progress.run = m_session->carry().run;
        std::string line = "TIME BONUS " + std::to_string(event.value);
        if (result.newlyFound != 0) {
            line += "   STAR COINS FOUND";
        } else if (result.bestScore && !result.firstClear) {
            line += "   NEW BEST SCORE";
        }
        if (!result.unlocked.empty() && m_level != game::kCastleLevel) {
            line += "   A NEW PATH OPENS";
        }
        m_banners.add(m_level == game::kCastleLevel ? "THE KING IS BEATEN!" : "COURSE CLEAR!", line, srgb(0xFFE14D), kClearSeconds);
        m_clearTimer = kClearSeconds;
        save();
        break;
    }
    case EventKind::Death:
        m_sfx.queue(sounds, Sfx::Death);
        m_music.stop(app.audio(), 0.3f);
        app.hitStop(0.12f);
        break;
    case EventKind::HurryUp:
        m_sfx.queue(sounds, Sfx::HurryUp);
        m_banners.add("HURRY UP!", "", srgb(0xFF6A3D), 1.6f);
        break;
    case EventKind::BossStart:
        m_banners.add(event.enemy == game::EnemyKind::King ? "THE KING" : "THE BRUTE", "STOMP HIM!", srgb(0xFF6A3D), 1.8f);
        break;
    case EventKind::BossHit:
        m_sfx.queue(sounds, Sfx::BossHit);
        app.hitStop(0.08f);
        break;
    case EventKind::BossDefeated:
        m_sfx.queue(sounds, Sfx::BossDefeated);
        m_music.stop(app.audio(), 0.3f);
        app.hitStop(0.25f);
        break;
    case EventKind::BossLand: m_sfx.queue(sounds, Sfx::BossLand); break;
    case EventKind::Points: break;
    case EventKind::PlatformDrop: m_sfx.queue(sounds, Sfx::PlatformDrop, 0.7f); break;
    }
}

void BounderGame::attemptOver(gamelib::app::App& app)
{
    const game::Carry carry = m_session->carry();
    if (m_session->outcome() == game::Outcome::Died) {
        m_profile.value().game.run = carry.run;
        if (carry.run.lives <= 0) {
            // Game over: back to the map with a fresh run once the menu is done; progress stays.
            m_gameOver = true;
            m_profile.value().game.run = {};
            m_sfx.queue(app.sounds(), Sfx::GameOver);
            m_menus.setGameOverTitle("GAME OVER");
            m_menus.gameOverAfter(0.4f, m_time);
            save();
            return;
        }
        save();
        startLevel(app, m_level, carry, false);
        m_card->line = carry.checkpoint ? "FROM THE CHECKPOINT" : "";
    }
}

void BounderGame::frame(gamelib::app::App& app, const gamelib::app::FrameInfo& info)
{
    m_time = info.time;
    m_transition.update(info.seconds);
    if (!m_transition.active()) {
        m_card.reset();
    }
    m_effects.update(info.seconds);
    m_demoEffects.update(info.seconds);
    m_banners.update(info.seconds);
    m_debug.frameTime(info.seconds);

    if (m_clearTimer > 0.0f && m_menus.is(ArcadeScreen::Playing)) {
        m_clearTimer -= info.seconds;
        if (m_clearTimer <= 0.0f) {
            if (m_level == game::kCastleLevel) {
                m_mode = Mode::Ending;
                m_endingTime = 0.0f;
                m_nextFirework = 0.5f;
                Progress& progress = m_profile.value().game;
                progress.finished = true;
                m_endingScore = progress.run.score;
                m_newBest = m_profile.value().bests.offer(gamelib::app::kScore, m_endingScore);
                save();
                m_music.play(app.audio(), Tune::Ending, 1.0f);
                m_menus.setGameOverTitle("THE END");
                m_menus.gameOverAfter(kEndingMenuDelay, m_time);
            } else {
                leaveLevel(app);
            }
        }
    }
    if (m_mode == Mode::Ending) {
        m_endingTime += info.seconds;
        m_nextFirework -= info.seconds;
        if (m_nextFirework <= 0.0f && m_session) {
            m_nextFirework = 0.5f;
            static constexpr std::array<std::uint32_t, 5> kColors{0xFFE14D, 0xFF6A3D, 0x5CE1FF, 0x7CFF6B, 0xFF5AB8};
            const int i = static_cast<int>(m_endingTime * 7.0f) % 5;
            const Vec2 center = m_view.view().toWorld({static_cast<float>(m_canvas.width) / 2.0f, 300.0f});
            const Vec2 at = center + Vec2{std::sin(m_endingTime * 2.7f) * 150.0f, std::cos(m_endingTime * 1.9f) * 40.0f};
            m_effects.firework(at, glow(srgb(kColors[static_cast<std::size_t>(i)]), 1.5f));
        }
    }
    if (m_menus.gameOverDue(m_time)) {
        m_menus.go(ArcadeScreen::GameOver, m_time);
        app.input().releaseAll();
    }
    if (m_session && m_mode == Mode::Level) {
        m_view.follow(*m_session, m_transition.active() || !m_menus.is(ArcadeScreen::Playing) ? 1.0f : info.alpha, info.seconds);
    }
    if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        m_demoView.follow(m_attract.sim().session, info.alpha, info.seconds);
    }
    updateMusic(app);
}

void BounderGame::updateMusic(gamelib::app::App& app)
{
    if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        m_music.play(app.audio(), Tune::Title, 0.6f);
        return;
    }
    if (m_mode == Mode::Map && !m_transition.active()) {
        m_music.play(app.audio(), Tune::Map, 0.6f);
        return;
    }
    if (m_mode != Mode::Level || !m_session || m_transition.active()) {
        return;
    }
    const game::Phase phase = m_session->phase();
    if (phase != game::Phase::Playing && phase != game::Phase::Growing && phase != game::Phase::PipeIn
        && phase != game::Phase::PipeOut) {
        return; // dying, the goal, the boss down: the jingles play alone
    }
    if (m_session->hero().starTicks > 60) {
        m_music.play(app.audio(), Tune::Star, 0.2f);
        return;
    }
    if (m_session->lockedArena()) {
        m_music.play(app.audio(), Tune::Boss, 0.4f);
        return;
    }
    m_music.play(app.audio(), _tuneFor(m_session->area().theme), 0.4f, m_session->time() <= game::kHurryUpTime ? kHurryTake : 0);
}

bool BounderGame::showsLevel() const
{
    return m_session && !m_demoMode && !m_menus.is(ArcadeScreen::Title) && m_mode != Mode::Map;
}

void BounderGame::draw(gamelib::app::App& app, gamelib::draw::DrawList& list, const gamelib::app::FrameInfo& info)
{
    (void)app;
    const gamelib::core::Size canvas = m_canvas;
    const bool shakeOn = m_profile.value().settings.screenShake;
    if (showsLevel()) {
        const bool frozen = !m_menus.is(ArcadeScreen::Playing) || m_transition.active();
        const Vec2 shake = shakeOn ? m_effects.shake(info.time) : Vec2{};
        m_view.draw(list, m_shapes, *m_session, m_effects,
                    {.alpha = frozen ? 1.0f : info.alpha, .shake = shake, .foundBefore = m_foundBefore});
        list.setView({});
        list.setLayer(gamelib::draw::layers::Hud);
        m_effects.drawPopups(list, m_view.view(shake));
        m_effects.drawFlash(list, canvas);
        drawHud(list, canvas, *m_session);
        drawBanners(list, canvas);
        if (m_mode == Mode::Ending) {
            drawEnding(list, canvas);
        }
        drawDebug(list, *m_session, m_view);
    } else if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        const DemoRun& run = m_attract.sim();
        m_demoView.draw(list, m_shapes, run.session, m_demoEffects, {.alpha = info.alpha});
        list.setView({});
        list.setLayer(gamelib::draw::layers::Hud);
        if (m_demoMode) {
            drawHud(list, canvas, run.session);
            list.text("DEMO", static_cast<float>(canvas.width) / 2.0f, static_cast<float>(canvas.height) - 80.0f,
                      {.pixelSize = 6.0f, .color = kText.withAlpha(0.6f + 0.4f * std::sin(info.time * 3.0f)),
                       .align = gamelib::draw::Align::Center, .shadow = true});
        }
        drawDebug(list, run.session, m_demoView);
    } else {
        const Progress& progress = m_profile.value().game;
        const game::WorldMap& map = game::worldMap(progress.world);
        drawMap(list, m_shapes, map, game::openCells(map, clearedTest()), progress.campaign,
                {.hero = m_mapHero, .facing = m_mapFacing, .walking = !m_route.empty(), .walked = m_mapWalked,
                 .node = m_route.empty() ? m_node : -1, .time = info.time},
                canvas);
        list.setLayer(gamelib::draw::layers::Hud);
        // The run: lives, coins, score, star coins found overall.
        const std::string runLine = "LIVES " + std::to_string(progress.run.lives) + "     COINS "
                                    + std::to_string(progress.run.coins) + "     SCORE " + std::to_string(progress.run.score)
                                    + "     STAR COINS " + std::to_string(starCoinsFound(progress.campaign)) + "/"
                                    + std::to_string(kStarCoinsTotal);
        list.text(runLine, static_cast<float>(canvas.width) / 2.0f, static_cast<float>(canvas.height) - 70.0f,
                  {.pixelSize = 4.0f, .color = kText, .align = gamelib::draw::Align::Center, .shadow = true});
    }
    list.setView({});
    list.setLayer(gamelib::draw::layers::Hud + 10);
    m_transition.draw(list, canvas);
    drawCard(list, canvas);
    if (!m_demoMode) {
        drawMenus(list, canvas);
    }
}

void BounderGame::drawHud(gamelib::draw::DrawList& list, const gamelib::core::Size canvas, const game::Session& session) const
{
    const game::RunState& run = session.run();
    const float w = static_cast<float>(canvas.width);
    const Color plate{0.04f, 0.03f, 0.08f, 0.55f};
    list.box({24.0f, 16.0f, w - 48.0f, 76.0f}, plate, {.cornerRadius = 18.0f});
    const gamelib::draw::TextStyle style{.pixelSize = 5.0f, .color = kText, .shadow = true};
    // Lives, with a little hero head.
    list.add({.rect = {44.0f, 16.0f, 72.0f, 73.5f}, .color = kHeroCap, .color2 = kHeroOveralls, .shape = m_shapes.hero,
              .params = {1.0f, 0.0f, 0.0f, 4.0f}, .scale = 3.0f});
    list.text("x" + std::to_string(run.lives), 120.0f, 36.0f, style);
    // Coins.
    list.add({.rect = {260.0f, 26.0f, 40.0f, 52.0f}, .color = kCoin, .color2 = kCoinLight, .shape = m_shapes.item,
              .params = {7.0f, 1.0f, 0.0f, 0.0f}, .scale = 3.0f});
    list.text("x" + std::to_string(run.coins), 310.0f, 36.0f, style);
    // Star coins of this level.
    for (int i = 0; i < 3; ++i) {
        const bool taken = (session.starCoinMask() & (1u << i)) != 0;
        const Vec2 c{520.0f + static_cast<float>(i) * 54.0f, 54.0f};
        list.circle(c, 20.0f, taken ? kCoin : Color{1.0f, 1.0f, 1.0f, 0.25f}, {.ringWidth = taken ? 0.0f : 3.0f});
        if (taken) {
            list.circle(c, 9.0f, srgb(0xFFF6C0));
        }
    }
    // Score.
    list.text(std::to_string(run.score), w / 2.0f + 120.0f, 36.0f,
              {.pixelSize = 5.0f, .color = kText, .align = gamelib::draw::Align::Center, .shadow = true});
    // Level and time.
    const game::LevelDef& def = session.def();
    list.text(std::string{def.id}, w - 420.0f, 36.0f, {.pixelSize = 5.0f, .color = srgb(0xFFE14D), .align = gamelib::draw::Align::Right, .shadow = true});
    const bool hurry = session.time() <= game::kHurryUpTime;
    const Color timeColor = hurry && std::fmod(m_time, 0.5f) < 0.25f ? srgb(0xFF6A3D) : kText;
    list.text("TIME " + std::to_string(session.time()), w - 52.0f, 36.0f,
              {.pixelSize = 5.0f, .color = timeColor, .align = gamelib::draw::Align::Right, .shadow = true});
    // The boss's health.
    if (const game::Enemy* boss = session.boss(); boss != nullptr && session.lockedArena()) {
        const int max = boss->kind == game::EnemyKind::King ? 12 : 6;
        const float barW = 600.0f;
        const float x = w / 2.0f - barW / 2.0f;
        list.box({x - 6.0f, 112.0f, barW + 12.0f, 34.0f}, plate, {.cornerRadius = 12.0f});
        list.box({x, 118.0f, barW * static_cast<float>(boss->hp) / static_cast<float>(max), 22.0f}, srgb(0xFF4A5A),
                 {.cornerRadius = 8.0f});
    }
}

void BounderGame::drawCard(gamelib::draw::DrawList& list, const gamelib::core::Size canvas) const
{
    // Fades in at the end of the cover and out at the start of the uncover
    // (coverage stays 1 through the hold), so it never sits over the level.
    const float a = gamelib::core::smoothstep(0.97f, 1.0f, m_transition.coverage());
    if (!m_card || a <= 0.0f) {
        return;
    }
    const float cx = static_cast<float>(canvas.width) / 2.0f;
    const float cy = static_cast<float>(canvas.height) / 2.0f;
    gamelib::draw::drawVectorText(list, m_card->title, cx, cy - 140.0f,
                                  {.size = 64.0f, .color = kText.withAlpha(a), .weight = 1.3f, .align = gamelib::draw::Align::Center});
    list.add({.rect = {cx - 130.0f, cy - 30.0f, 72.0f, 73.5f}, .color = kHeroCap.withAlpha(a), .color2 = kHeroOveralls.withAlpha(a),
              .shape = m_shapes.hero, .params = {1.0f, 0.0f, 0.0f, 4.0f}, .scale = 3.0f});
    list.text("x " + std::to_string(m_card->lives), cx - 40.0f, cy - 6.0f, {.pixelSize = 7.0f, .color = kText.withAlpha(a)});
    if (!m_card->line.empty()) {
        list.text(m_card->line, cx, cy + 110.0f, {.pixelSize = 4.0f, .color = kTextDim.withAlpha(a), .align = gamelib::draw::Align::Center});
    }
}

void BounderGame::drawBanners(gamelib::draw::DrawList& list, const gamelib::core::Size canvas) const
{
    m_banners.draw(list, static_cast<float>(canvas.width) / 2.0f,
                   {.top = static_cast<float>(canvas.height) * 0.3f, .lineColor = kText, .shadow = kInk.withAlpha(0.6f)});
}

void BounderGame::drawEnding(gamelib::draw::DrawList& list, const gamelib::core::Size canvas) const
{
    if (m_menus.is(ArcadeScreen::GameOver)) {
        return;
    }
    const Progress& progress = m_profile.value().game;
    const float a = gamelib::core::smoothstep(0.5f, 1.5f, m_endingTime);
    const float cx = static_cast<float>(canvas.width) / 2.0f;
    list.box({cx - 620.0f, 360.0f, 1240.0f, 470.0f}, kPanel.withAlpha(0.82f * a),
             {.cornerRadius = 28.0f, .borderWidth = 4.0f, .borderColor = srgb(0xFFC93C).withAlpha(a)});
    gamelib::draw::drawVectorText(list, "The kingdom is free!", cx, 400.0f,
                                  {.size = 72.0f, .color = srgb(0xFFE14D).withAlpha(a), .weight = 1.4f,
                                   .align = gamelib::draw::Align::Center});
    const float b = gamelib::core::smoothstep(1.5f, 2.5f, m_endingTime);
    const std::array<std::string, 4> lines{
        "SCORE   " + std::to_string(m_endingScore),
        "LEVELS CLEARED   " + std::to_string(progress.campaign.clearedCount()) + " / " + std::to_string(game::kLevelCount),
        "STAR COINS   " + std::to_string(starCoinsFound(progress.campaign)) + " / " + std::to_string(kStarCoinsTotal),
        m_newBest ? std::string{"NEW BEST SCORE!"} : "BEST SCORE   " + std::to_string(m_profile.value().bests.get(gamelib::app::kScore).value_or(0)),
    };
    const float thanks = gamelib::core::smoothstep(4.5f, 5.5f, m_endingTime);
    gamelib::draw::drawVectorText(list, "Thanks for playing Bounder!", cx, 880.0f,
                                  {.size = 48.0f, .color = kText.withAlpha(thanks), .weight = 1.2f, .align = gamelib::draw::Align::Center});
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float fade = gamelib::core::smoothstep(1.5f + 0.4f * static_cast<float>(i), 2.3f + 0.4f * static_cast<float>(i), m_endingTime);
        list.text(lines[i], cx, 540.0f + 66.0f * static_cast<float>(i),
                  {.pixelSize = 6.0f, .color = (i == 3 && m_newBest ? srgb(0xFFE14D) : kText).withAlpha(fade * b),
                   .align = gamelib::draw::Align::Center, .shadow = true});
    }
}

void BounderGame::drawMenus(gamelib::draw::DrawList& list, const gamelib::core::Size canvas) const
{
    const gamelib::ui::Menu* menu = m_menus.menu();
    if (menu == nullptr) {
        return;
    }
    const Progress& progress = m_profile.value().game;
    std::string subtitle;
    std::string footer;
    if (m_menus.is(ArcadeScreen::Title)) {
        subtitle = "LEVELS " + std::to_string(progress.campaign.clearedCount()) + "/" + std::to_string(game::kLevelCount)
                   + "  STARS " + std::to_string(starCoinsFound(progress.campaign)) + "/" + std::to_string(kStarCoinsTotal);
        footer = "ARROWS MOVE   Z JUMP   X RUN AND FIRE   DOWN PIPES   ESC PAUSE";
    } else if (m_menus.is(ArcadeScreen::GameOver)) {
        const std::optional<std::int64_t> best = m_profile.value().bests.get(gamelib::app::kScore);
        subtitle = m_mode == Mode::Ending ? "SCORE " + std::to_string(m_endingScore) : "PROGRESS IS SAVED";
        if (best) {
            footer = "BEST SCORE " + std::to_string(*best);
        }
    } else if (m_menus.is(ArcadeScreen::Paused) && m_session && m_mode == Mode::Level) {
        subtitle = std::string{m_session->def().id} + "  " + std::string{m_session->def().name};
        footer = "RESTART PLAYS THE LEVEL AGAIN   EXIT TO MAP LEAVES IT";
    }
    gamelib::ui::drawMenu(list, *menu, canvas, _menuStyle(), subtitle, footer);
}

void BounderGame::drawDebug(gamelib::draw::DrawList& list, const game::Session& session, const LevelView& view)
{
    if (!m_debug.enabled()) {
        m_debug.draw(list);
        return;
    }
    const gamelib::core::View2D v = view.view();
    m_debug.box(v.rectToCanvas(session.hero().body.box), Color{0.3f, 1.0f, 0.4f, 1.0f});
    for (const auto [handle, e] : session.area().enemies) {
        m_debug.box(v.rectToCanvas(e.box), e.awake ? Color{1.0f, 0.3f, 0.3f, 1.0f} : Color{0.6f, 0.6f, 0.6f, 1.0f});
    }
    const auto& hero = session.hero().body;
    m_debug.stat("LEVEL", std::string{session.def().id} + " AREA " + std::to_string(session.areaIndex()));
    m_debug.stat("POS", std::to_string(static_cast<int>(hero.box.x)) + "," + std::to_string(static_cast<int>(hero.box.y)));
    m_debug.stat("VEL", std::to_string(hero.velocity.x).substr(0, 5) + "," + std::to_string(hero.velocity.y).substr(0, 5));
    m_debug.stat("STATE", std::string{hero.grounded ? "GROUND" : "AIR"} + " COYOTE " + std::to_string(hero.coyote)
                              + " PHASE " + std::to_string(static_cast<int>(session.phase())));
    m_debug.draw(list);
}

} // namespace bounder::app
