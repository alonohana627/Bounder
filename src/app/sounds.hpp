#pragma once

// Bounder's sound effects and music, all synthesized with GameLib's 8-bit
// synth at startup: no audio files. Every world has a theme, and the level
// themes have a faster take for when the timer runs low.

#include <gamelib/audio/music.hpp>
#include <gamelib/audio/player.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
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

[[nodiscard]] gamelib::audio::Song song(Tune tune, bool hurry);

// The rendered tunes, registered as music tracks.
class Music {
public:
    explicit Music(gamelib::audio::Player& player);
    // Crossfades to a tune (its hurry-up take if asked and it has one);
    // nothing if it already plays.
    void play(gamelib::audio::Player& player, Tune tune, bool hurry = false, float fadeSeconds = 0.6f) const;
    void stop(gamelib::audio::Player& player) const;

private:
    std::array<gamelib::audio::SoundId, kTuneCount> m_tracks{};
    std::array<gamelib::audio::SoundId, kHurryTunes> m_hurry{};
};

} // namespace bounder::app
