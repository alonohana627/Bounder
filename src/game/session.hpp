#pragma once

// One attempt at one level: the hero (GameLib's platformer controller with
// Bounder's walk/run tuning), power-ups, blocks, coins and star coins,
// enemies, moving and falling platforms, pipes to bonus rooms, fireballs,
// the checkpoint, the goal pole, the timer and the bosses. Deterministic:
// the same Carry and per-tick Input give the same attempt, so the demo bot
// and recordings replay exactly. No window, audio or clock.
//
// An attempt ends in Phase::Over with an Outcome: Cleared (the goal or a
// boss) or Died; the caller starts the next attempt with the Carry it
// returns (lives, coins, score and power go on).

#include "levels.hpp"
#include "rules.hpp"

#include <gamelib/core/combat.hpp>
#include <gamelib/core/event_queue.hpp>
#include <gamelib/core/math.hpp>
#include <gamelib/core/platformer.hpp>
#include <gamelib/core/projectiles.hpp>
#include <gamelib/core/rng.hpp>
#include <gamelib/core/slot_map.hpp>
#include <gamelib/core/state_machine.hpp>
#include <gamelib/core/tile_collision.hpp>
#include <gamelib/core/tile_map.hpp>
#include <gamelib/core/waypoint_path.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace bounder::game {

// What goes from one attempt (and one level) to the next.
struct RunState {
    int lives = kStartLives;
    int coins = 0;
    std::int64_t score = 0;
    Power power = Power::Small;

    friend bool operator==(const RunState&, const RunState&) = default;
};

// The start of an attempt.
struct Carry {
    RunState run;
    bool checkpoint = false;     // start at the checkpoint flag
    std::uint8_t starCoins = 0;  // star coins taken earlier in this level (kept through deaths)
    std::int64_t levelPoints = 0; // points earned in this level so far (kept through deaths)
};

enum class Phase {
    Playing,
    Growing,  // the freeze while the hero grows, shrinks or takes the flower
    PipeIn,   // sinking into a pipe
    PipeOut,  // rising out of a pipe
    Dying,    // the death hop
    Goal,     // sliding down the pole and walking off
    BossDown, // the boss is beaten: a short pause before the clear
    Over,     // the attempt ended (see outcome())
};

enum class Outcome { None, Cleared, Died };

enum class EnemyKind : std::uint8_t { Walker, Shell, RedShell, Hopper, Flier, FlierVertical, Plant, Spiky, Brute, King };
enum class EnemyState : std::uint8_t {
    Walking,      // alive and moving by its kind's rule
    ShellIdle,    // a stomped shell enemy, waiting to be kicked
    ShellSliding, // a kicked shell
    Squashed,     // stomped flat: shown a moment, then gone
    Knocked,      // hit by a fireball, shell, star or bump: flips and falls out
};

enum class ItemKind : std::uint8_t { Mushroom, Flower, Star, OneUp };

enum class BossState : std::uint8_t { Idle, Walk, Hop, Leap, Throw, Stunned, Defeated };

struct Enemy {
    EnemyKind kind = EnemyKind::Walker;
    EnemyState state = EnemyState::Walking;
    gamelib::core::Rect box;
    gamelib::core::Rect previous; // last tick's box, for interpolation
    gamelib::core::Vec2 velocity;
    gamelib::core::Vec2 origin;   // spawn point (fliers, plants)
    int dir = -1;
    bool awake = false;
    bool grounded = false;
    int age = 0;       // ticks since waking
    int timer = 0;     // ticks in the current state
    int kickGrace = 0; // a kicked shell does not hurt the kicker for a few ticks
    int chain = 0;     // enemies a sliding shell has knocked out
    int hp = 1;        // bosses
    int hurtTicks = 0; // bosses: flashing after a hit, cannot be hit again
    int pattern = -1;  // the King: the last attack pattern
    gamelib::core::StateMachine<BossState> boss{BossState::Idle};
    gamelib::core::Vulnerable vulnerable;
};

struct Item {
    ItemKind kind = ItemKind::Mushroom;
    gamelib::core::Rect box;
    gamelib::core::Rect previous;
    gamelib::core::Vec2 velocity;
    int rise = 0; // ticks left rising out of the block
    int dir = 1;
    int age = 0;
};

// A pipe the hero enters by holding down while standing on its top (cell =
// the top's left tile), and leaves by rising out of the target pipe.
struct PipeLink {
    gamelib::core::Cell top;
    int targetArea = 0;
    gamelib::core::Cell targetTop;
};

inline constexpr float kPlatformSpeed = 0.7f; // units per tick along a rail

struct Platform {
    gamelib::core::WaypointPath path; // the rail (top-left corner positions); no points: a falling platform
    float startOffset = 0.0f;         // where on the rail it starts
    gamelib::core::Rect box;
    gamelib::core::Rect previous;
    gamelib::core::Vec2 delta;
    int id = 0;
    bool falling = false;  // a falling platform...
    int standTicks = 0;    // ...stood on this long...
    bool dropping = false; // ...and now dropping
    float fallSpeed = 0.0f;
};

struct Bump {
    gamelib::core::Cell cell;
    int ticks = 0; // since the hit
};

struct CoinBrick {
    gamelib::core::Cell cell;
    int coinsLeft = 8;
    int timer = 0; // ticks since the first hit: it runs dry after a while
    bool started = false;
};

struct StarCoin {
    int index = 0; // 0..2
    int area = 0;
    gamelib::core::Vec2 center;
    bool taken = false;
};

struct BossShotData {
    bool boss = false; // a boss's fireball (hurts the hero) rather than the hero's
};
using Shots = gamelib::core::Projectiles<BossShotData>;

struct Area {
    gamelib::core::TileMap map{1, 1, kTile};
    gamelib::core::SlotMap<Enemy> enemies;
    std::vector<Item> items;
    std::vector<Platform> platforms;
    std::vector<Bump> bumps;
    std::vector<CoinBrick> coinBricks;
    std::vector<PipeLink> pipes;
    gamelib::core::Cell checkpoint{-1, -1};
    gamelib::core::Cell goal{-1, -1};
    Theme theme = Theme::Grass;
};

struct Hero {
    gamelib::core::PlatformerBody body;
    gamelib::core::Rect previous;
    Power power = Power::Small;
    Power growFrom = Power::Small; // the power before the current Growing phase
    gamelib::core::Vulnerable vulnerable;
    int starTicks = 0;
    int chain = 0;       // stomps in a row without landing
    int throwTicks = 0;  // for the throwing pose
    bool running = false;
    bool skidding = false;
    float walked = 0.0f; // distance walked on the ground, for the walk cycle
};

enum class EventKind {
    Jump,
    Land,
    Skid,
    Stomp,        // value: chain step
    Kick,
    EnemyKnocked, // an enemy knocked out by a shell, fireball, star or a bump
    Coin,
    BlockBump,
    BrickBreak,
    ItemSpawn,    // item
    PowerUp,      // item taken
    OneUp,
    Hurt,         // shrank
    Fireball,
    FireballPop,  // a fireball hit a wall or an enemy
    PipeIn,
    PipeOut,
    Checkpoint,
    StarCoin,     // value: index
    GoalTouch,    // value: points
    Clear,
    Death,
    HurryUp,
    BossStart,
    BossHit,      // value: hp left
    BossDefeated,
    BossLand,     // a heavy landing: shake
    Points,       // value: points (0 = 1UP), shown as a popup
    PlatformDrop,
};

struct Event {
    EventKind kind = EventKind::Jump;
    gamelib::core::Vec2 at;
    int value = 0;
    ItemKind item = ItemKind::Mushroom;
    EnemyKind enemy = EnemyKind::Walker;
};

class Session {
public:
    Session(int level, const Carry& carry);

    void tick(const Input& input);
    // A developer's start further into the level (BOUNDER_START_COLUMN):
    // the hero stands on the highest floor of that column.
    void debugStartAt(int column);
    // A developer's start standing on the floor of a cell (tests).
    void debugPlace(gamelib::core::Cell cell);

    [[nodiscard]] int level() const { return m_level; }
    [[nodiscard]] const LevelDef& def() const { return game::level(m_level); }
    [[nodiscard]] Phase phase() const { return m_phase; }
    [[nodiscard]] Outcome outcome() const { return m_outcome; }
    [[nodiscard]] int phaseTicks() const { return m_phaseTicks; }
    // The Carry for the next attempt (after Over): lives, coins, score,
    // power, the checkpoint and the star coins taken.
    [[nodiscard]] Carry carry() const;

    [[nodiscard]] const RunState& run() const { return m_run; }
    [[nodiscard]] const Hero& hero() const { return m_hero; }
    [[nodiscard]] int areaIndex() const { return m_area; }
    [[nodiscard]] const Area& area() const { return m_areas[static_cast<std::size_t>(m_area)]; }
    [[nodiscard]] const gamelib::core::TileMap& map() const { return area().map; }
    [[nodiscard]] gamelib::core::TileWorld tileWorld() const;
    [[nodiscard]] const Shots& shots() const { return m_shots; }
    [[nodiscard]] const std::vector<StarCoin>& starCoins() const { return m_starCoins; }
    [[nodiscard]] bool checkpointReached() const { return m_checkpointReached; }
    [[nodiscard]] int time() const { return m_time; }         // timer units left
    [[nodiscard]] int ticks() const { return m_ticks; }       // ticks played in this attempt
    [[nodiscard]] std::int64_t levelPoints() const { return m_levelPoints; }
    [[nodiscard]] std::uint8_t starCoinMask() const;
    // The goal pole's flag height (0 top .. 1 bottom) while sliding.
    [[nodiscard]] float goalFlag() const { return m_goalFlag; }
    // The boss arena (world units) once the hero entered it and it closed.
    [[nodiscard]] std::optional<gamelib::core::Rect> lockedArena() const;
    [[nodiscard]] const Enemy* boss() const;
    // The pipe the hero is in during PipeIn / PipeOut.
    [[nodiscard]] gamelib::core::Vec2 pipeCenter() const { return m_pipeCenter; }

    [[nodiscard]] gamelib::core::EventQueue<Event>& events() { return m_events; }
    [[nodiscard]] std::uint64_t hash() const;

private:
    [[nodiscard]] Area& currentArea() { return m_areas[static_cast<std::size_t>(m_area)]; }
    void loadAreas();
    void spawnHero(gamelib::core::Vec2 feet);
    void setPower(Power power);

    // session.cpp
    void playTick(const Input& input);
    void stepHero(const Input& input);
    void hitBlockAbove(float topBefore, float vyBefore, bool hitHead);
    void hitBlock(gamelib::core::Cell cell);
    void bumpAbove(gamelib::core::Cell cell);
    void collectCoins();
    void collectStarCoins();
    void checkPipes(const Input& input);
    void checkCheckpointAndGoal();
    void throwFireball();
    void stepShots();
    void stepItems();
    void stepPlatforms();
    void stepTimer();
    void hurtHero();
    void die();
    void addPoints(int points, gamelib::core::Vec2 at);
    void addChainPoints(int& chain, gamelib::core::Vec2 at);
    void addCoin(gamelib::core::Vec2 at);
    void addLife(gamelib::core::Vec2 at);
    void enterPipe(const PipeLink& link);
    void linkPipes();
    void goalTick(const Input& input);
    void dyingTick();

    // enemies.cpp
    void spawnEnemies(Area& area, int areaIndex);
    void stepEnemies();
    void stepEnemy(Enemy& enemy);
    void heroMeetsEnemies(float heroFall);
    void knockOut(Enemy& enemy, int dir);
    void shellHits(Enemy& shell);

    // boss.cpp
    void stepBoss(Enemy& boss);
    void hitBoss(Enemy& boss, int damage, gamelib::core::Vec2 at);
    void checkArena();

    int m_level = 0;
    Carry m_start;
    RunState m_run;
    std::vector<Area> m_areas;
    int m_area = 0;
    Hero m_hero;
    Shots m_shots{16};
    std::vector<StarCoin> m_starCoins;
    gamelib::core::Rng m_rng;
    Phase m_phase = Phase::Playing;
    Outcome m_outcome = Outcome::None;
    int m_phaseTicks = 0;
    int m_ticks = 0;
    int m_time = 300;
    int m_timeTicks = 0;
    bool m_hurried = false;
    bool m_checkpointReached = false;
    std::int64_t m_levelPoints = 0;
    float m_goalFlag = 0.0f;
    bool m_arenaClosed = false;
    int m_arenaColumn = -1; // the boss arena's gate column (a 'z' marker), -1 without a boss
    // Pipes: the link being used and where the hero is going.
    std::optional<PipeLink> m_pipe;
    gamelib::core::Vec2 m_pipeCenter;
    gamelib::core::EventQueue<Event> m_events;
};

} // namespace bounder::game
