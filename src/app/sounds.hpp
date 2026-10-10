#pragma once

// Bounder's sound effects and music, all synthesized with GameLib's 8-bit
// synth at startup: no audio files. Every world has a theme, and the level
// themes have a faster take for when the timer runs low.

#include <gamelib/audio/music.hpp>
#include <gamelib/audio/player.hpp>
#include <gamelib/audio/sound_bank.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace bounder::app {

enum class Sfx : std::uint8_t {
    Jump,
    BigJump,
    Stomp,
    Kick,
    Coin,
    Sprout,     // a power-up rises out of its block
    PowerUp,
    OneUp,
    Bump,
    BrickBreak,
    Pipe,
    Shrink,
    Fireball,
    FireballPop,
    Skid,
    Checkpoint,
    Pole,
    Clear,
    Death,
    HurryUp,
    StarCoin,
    BossHit,
    BossDefeated,
    BossLand,
    PlatformDrop,
    MapMove,
    MapEnter,
    MenuMove,
    MenuSelect,
    MenuBack,
    GameOver,
    Count
};
inline constexpr std::size_t kSfxCount = static_cast<std::size_t>(Sfx::Count);

[[nodiscard]] std::vector<float> renderSfx(Sfx sfx, std::uint32_t sampleRate);

enum class Tune : std::uint8_t { Grass, Desert, Ice, Sky, Fortress, Bonus, Map, Boss, Title, Ending, Star, Count };
inline constexpr std::size_t kTuneCount = static_cast<std::size_t>(Tune::Count);
// The tunes with a hurry-up take: the level themes (Grass .. Bonus).
inline constexpr std::size_t kHurryTunes = 6;

// Take 0 of a tune, or take 1 (the hurry-up take, a quarter faster) of the
// level themes; nullopt for a take a tune does not have.
[[nodiscard]] std::optional<gamelib::audio::Song> tuneTake(Tune tune, int take);
inline constexpr int kHurryTake = 1;

// The rendered tunes and their hurry-up takes.
using Music = gamelib::audio::MusicBank<Tune, kTuneCount>;

} // namespace bounder::app
