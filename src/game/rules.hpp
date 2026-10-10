#pragma once

// Bounder's numbers: the tile grid, the tick rate, the hero's sizes and the
// movement tuning (walk, run, and a jump that grows with the run speed),
// scoring, timers and the per-tick input. Every rule runs on whole ticks at
// kTickRate, so the same per-tick input always gives the same run.

#include <gamelib/core/math.hpp>
#include <gamelib/core/platformer.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bounder::game {

inline constexpr int kTickRate = 60;
inline constexpr float kTile = 16.0f;   // world units per tile
inline constexpr int kViewRows = 15;    // the camera shows 15 rows (240 units) and about 27 columns
inline constexpr float kViewHeight = kViewRows * kTile;

// The hero's collision boxes by size (the drawing is a little larger).
inline constexpr gamelib::core::Vec2 kSmallSize{11.0f, 14.0f};
inline constexpr gamelib::core::Vec2 kBigSize{12.0f, 26.0f};

enum class Power : std::uint8_t { Small, Big, Fire };

// Timers, in ticks.
inline constexpr int kHurtInvincibleTicks = 120; // after shrinking
inline constexpr int kStarTicks = 600;           // star power
inline constexpr int kGrowTicks = 36;            // the growing / shrinking freeze
inline constexpr int kDeathTicks = 150;          // the death hop and fall before the attempt ends
inline constexpr int kPipeTicks = 32;            // sliding into or out of a pipe
inline constexpr int kTimeUnitTicks = 24;        // one unit of the level timer (0.4 s, as in the classics)
inline constexpr int kHurryUpTime = 100;         // timer units left when the music speeds up
inline constexpr int kShellReviveTicks = 420;    // an idle shell walks again after this
inline constexpr int kItemRiseTicks = 20;        // a power-up rises out of its block

// Movement speeds (units per tick).
inline constexpr float kWalkSpeed = 1.5f;
inline constexpr float kRunSpeed = 2.6f;
inline constexpr float kStompBounce = 4.4f;   // stomp launch speed (cut by releasing jump)
inline constexpr float kShellSpeed = 3.2f;
inline constexpr float kWalkerSpeed = 0.5f;
inline constexpr float kEnemyGravity = 0.22f;
inline constexpr float kEnemyMaxFall = 4.0f;

// Points.
inline constexpr int kCoinPoints = 50;
inline constexpr int kBrickPoints = 50;
inline constexpr int kPowerUpPoints = 1000;
inline constexpr int kFireballKillPoints = 200;
inline constexpr int kStarCoinPoints = 2000;
inline constexpr int kTimeBonusPerUnit = 50;
inline constexpr int kCoinsPerLife = 100;
inline constexpr int kStartLives = 5;
inline constexpr int kMaxLives = 99;
// Stomps (and shell kills) in a row without landing: 100, 200, 400 ... then
// a life per stomp. A value of 0 means "1UP".
inline constexpr std::array<int, 10> kChainPoints{100, 200, 400, 500, 800, 1000, 2000, 4000, 8000, 0};

// One tick of player input, as recorded for replays: after key mapping,
// never raw keys.
struct Input {
    int move = 0;             // -1 left, 0, 1 right
    bool down = false;        // into pipes
    bool jumpHeld = false;    // held this tick
    bool jumpPressed = false; // pressed this tick (also held)
    bool runHeld = false;     // run (and throw while on fire)
    bool runPressed = false;  // pressed this tick: throws a fireball with the flower

    friend bool operator==(const Input&, const Input&) = default;
};

// Four characters per tick: move L/R/., down d/., jump h/p/., run r/f/.
// (p and f are "pressed this tick", which implies held).
[[nodiscard]] std::string encodeInput(const Input& input);
[[nodiscard]] std::optional<Input> decodeInput(std::string_view text);

// Bumped whenever a rule or a world 1 level changes, so stale recordings are refused.
inline constexpr std::uint32_t kRulesVersion = 1;

// The movement for this tick: walking or running, with the jump height
// growing with the current horizontal speed (a full-speed run jumps a
// tile higher), a faster cap running down a slope, and slippery footing on ice.
[[nodiscard]] gamelib::core::PlatformerTuning tuningFor(bool running, float speedX, bool downhill, bool icy);

// The hero's box size for a power state.
[[nodiscard]] gamelib::core::Vec2 heroSize(Power power);

} // namespace bounder::game
