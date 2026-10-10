#include "sounds.hpp"

#include <gamelib/audio/synth.hpp>

namespace bounder::app {

namespace {

// helpers

using gamelib::audio::Note;
using gamelib::audio::Song;
using gamelib::audio::Track;
using gamelib::audio::Wave;
using gamelib::audio::noise;
using gamelib::audio::tone;

// MIDI note numbers (C4 = 60).
constexpr float kC3 = 48, kE3 = 52, kG3 = 55, kA3 = 57, kC4 = 60, kD4 = 62, kE4 = 64, kF4 = 65, kG4 = 67, kA4 = 69;
constexpr float kB4 = 71, kC5 = 72, kD5 = 74, kE5 = 76, kG5 = 79, kB5 = 83, kC6 = 84, kE6 = 88;
constexpr float kG6 = 91, kC7 = 96;

struct Recipe {
    std::vector<std::vector<Note>> tracks;
    float peakDb = -14.0f;
};

Recipe _recipe(const Sfx sfx)
{
    switch (sfx) {
    // The classic jump: a quick rising sweep.
    case Sfx::Jump: return {{{tone(Wave::Pulse25, kC5 - 3.0f, 0.13f, kC6 + 2.0f)}}, -19.0f};
    // A bigger body jumps a little lower and rounder.
    case Sfx::BigJump: return {{{tone(Wave::Pulse50, kG4, 0.15f, kG5 + 2.0f)}}, -19.0f};
    // A squishy thump with a little hop up.
    case Sfx::Stomp:
        return {{{tone(Wave::Pulse25, kC5, 0.05f, kG4), tone(Wave::Pulse25, kG5, 0.07f, kC6)},
                 {noise(2600.0f, 0.06f, 900.0f, 0.7f)}},
                -15.0f};
    // A hard knock: the shell flies.
    case Sfx::Kick: return {{{tone(Wave::Pulse12, kG5, 0.04f), tone(Wave::Pulse12, kC6, 0.06f)}, {noise(5000.0f, 0.05f, 1500.0f, 0.8f)}}, -16.0f};
    // Two bright notes: B5 then E6.
    case Sfx::Coin: return {{{tone(Wave::Pulse25, kB5, 0.05f), tone(Wave::Pulse25, kE6, 0.22f)}}, -18.0f};
    // A power-up climbing out of its block.
    case Sfx::Sprout:
        return {{{tone(Wave::Pulse50, kC4, 0.06f, kG4), tone(Wave::Pulse50, kD4, 0.06f, kA4), tone(Wave::Pulse50, kE4, 0.06f, kB4),
                  tone(Wave::Pulse50, kF4, 0.06f, kC5), tone(Wave::Pulse50, kG4, 0.06f, kD5)}},
                -18.0f};
    // Growing: a shimmering rising arpeggio.
    case Sfx::PowerUp:
        return {{{tone(Wave::Pulse25, kC5, 0.05f), tone(Wave::Pulse25, kG4, 0.05f), tone(Wave::Pulse25, kC5, 0.05f),
                  tone(Wave::Pulse25, kE5, 0.05f), tone(Wave::Pulse25, kG5, 0.05f), tone(Wave::Pulse25, kC6, 0.05f),
                  tone(Wave::Pulse25, kE6, 0.05f), tone(Wave::Pulse25, kG6, 0.12f)}},
                -15.0f};
    // A life: a happy six-note run.
    case Sfx::OneUp:
        return {{{tone(Wave::Pulse25, kE5, 0.09f), tone(Wave::Pulse25, kG5, 0.09f), tone(Wave::Pulse25, kE6, 0.09f),
                  tone(Wave::Pulse25, kC6, 0.09f), tone(Wave::Pulse25, kD5 + 12.0f, 0.09f), tone(Wave::Pulse25, kG6, 0.18f)}},
                -14.0f};
    // A dull knock on a block that does not give.
    case Sfx::Bump: return {{{tone(Wave::Triangle, kG3, 0.08f, kC3)}, {noise(1200.0f, 0.05f, 500.0f, 0.6f)}}, -16.0f};
    // A brick shattering: crackle and tumbling bits.
    case Sfx::BrickBreak:
        return {{{noise(3800.0f, 0.06f, 1200.0f), noise(2400.0f, 0.08f, 700.0f, 0.8f), noise(1600.0f, 0.1f, 400.0f, 0.6f)},
                 {tone(Wave::Triangle, kC4, 0.1f, kC3)}},
                -14.0f};
    // Into a pipe: a falling warble.
    case Sfx::Pipe:
        return {{{tone(Wave::Pulse50, kG4, 0.08f, kC4), tone(Wave::Pulse50, kG4, 0.08f, kC4), tone(Wave::Pulse50, kG4, 0.08f, kC4)}},
                -17.0f};
    // Hurt and shrinking: falling steps.
    case Sfx::Shrink:
        return {{{tone(Wave::Pulse25, kE5, 0.06f), tone(Wave::Pulse25, kC5, 0.06f), tone(Wave::Pulse25, kA4, 0.06f),
                  tone(Wave::Pulse25, kE4, 0.06f), tone(Wave::Pulse25, kC4, 0.12f)}},
                -15.0f};
    case Sfx::Fireball: return {{{tone(Wave::Pulse12, kC6, 0.05f, kC5)}, {noise(7000.0f, 0.05f, 3000.0f, 0.5f)}}, -19.0f};
    case Sfx::FireballPop: return {{{noise(4500.0f, 0.06f, 1500.0f)}}, -21.0f};
    case Sfx::Skid: return {{{noise(9000.0f, 0.12f, 6000.0f, 0.6f)}}, -24.0f};
    // The checkpoint: a bright chime.
    case Sfx::Checkpoint:
        return {{{tone(Wave::Pulse25, kG5, 0.08f), tone(Wave::Pulse25, kC6, 0.08f), tone(Wave::Pulse25, kE6, 0.25f)},
                 {tone(Wave::Triangle, kC4, 0.4f)}},
                -14.0f};
    // Sliding down the goal pole: a long falling sweep.
    case Sfx::Pole: return {{{tone(Wave::Pulse12, kC7, 0.9f, kC5)}}, -22.0f};
    // The level is cleared: a short fanfare.
    case Sfx::Clear:
        return {{{tone(Wave::Pulse25, kG4, 0.11f), tone(Wave::Pulse25, kC5, 0.11f), tone(Wave::Pulse25, kE5, 0.11f),
                  tone(Wave::Pulse25, kG5, 0.22f), tone(Wave::Pulse25, kE5, 0.11f), tone(Wave::Pulse25, kG5, 0.6f)},
                 {tone(Wave::Pulse12, kE4, 0.33f), tone(Wave::Pulse12, kG4, 0.33f), tone(Wave::Pulse12, kC5, 0.6f)},
                 {tone(Wave::Triangle, kC3, 0.33f), tone(Wave::Triangle, kG3, 0.33f), tone(Wave::Triangle, kC3, 0.6f)}},
                -11.0f};
    // A life lost: a sad little tune.
    case Sfx::Death:
        return {{{tone(Wave::Pulse25, kC5, 0.12f), tone(Wave::Pulse25, kB4, 0.12f), tone(Wave::Pulse25, kA4, 0.12f),
                  tone(Wave::Pulse25, kG4, 0.24f), tone(Wave::Pulse25, kE4, 0.2f), tone(Wave::Pulse25, kC4, 0.5f)},
                 {tone(Wave::Triangle, kC3, 0.36f), tone(Wave::Triangle, kG3 - 12.0f, 0.44f), tone(Wave::Triangle, kC3 - 12.0f, 0.5f)}},
                -12.0f};
    // The timer runs low: an alarm.
    case Sfx::HurryUp:
        return {{{tone(Wave::Pulse25, kE5, 0.1f), tone(Wave::Pulse25, kC5, 0.1f), tone(Wave::Pulse25, kE5, 0.1f),
                  tone(Wave::Pulse25, kC5, 0.1f), tone(Wave::Pulse25, kE5, 0.1f), tone(Wave::Pulse25, kC5, 0.25f)}},
                -13.0f};
    // A star coin: a sparkling cascade.
    case Sfx::StarCoin:
        return {{{tone(Wave::Pulse25, kC6, 0.06f), tone(Wave::Pulse25, kG5, 0.06f), tone(Wave::Pulse25, kE6, 0.06f),
                  tone(Wave::Pulse25, kC6, 0.06f), tone(Wave::Pulse25, kG6, 0.3f)},
                 {tone(Wave::Pulse12, kE5, 0.12f), tone(Wave::Pulse12, kG5, 0.12f), tone(Wave::Pulse12, kC6, 0.3f)}},
                -13.0f};
    case Sfx::BossHit:
        return {{{tone(Wave::Pulse50, kG3, 0.18f, kC3)}, {noise(3000.0f, 0.15f, 300.0f)}}, -12.0f};
    case Sfx::BossDefeated:
        return {{{noise(5000.0f, 0.5f, 120.0f)},
                 {tone(Wave::Pulse25, kC5, 0.15f, kC4), tone(Wave::Pulse25, kG4, 0.15f, kG3), tone(Wave::Pulse25, kE4, 0.4f, kC3)}},
                -11.0f};
    case Sfx::BossLand: return {{{noise(900.0f, 0.25f, 150.0f)}, {tone(Wave::Triangle, kC3, 0.2f, kC3 - 12.0f)}}, -13.0f};
    case Sfx::PlatformDrop: return {{{tone(Wave::Triangle, kE3, 0.18f, kC3 - 5.0f)}, {noise(800.0f, 0.12f, 300.0f, 0.5f)}}, -18.0f};
    case Sfx::MapMove: return {{{tone(Wave::Pulse12, kG5, 0.03f), tone(Wave::Pulse12, kC6, 0.04f)}}, -22.0f};
    case Sfx::MapEnter:
        return {{{tone(Wave::Pulse25, kC5, 0.06f), tone(Wave::Pulse25, kE5, 0.06f), tone(Wave::Pulse25, kG5, 0.06f),
                  tone(Wave::Pulse25, kC6, 0.15f)}},
                -15.0f};
    case Sfx::MenuMove: return {{{tone(Wave::Pulse12, kC7, 0.03f)}}, -21.0f};
    case Sfx::MenuSelect: return {{{tone(Wave::Pulse25, kC5, 0.04f), tone(Wave::Pulse25, kG5, 0.07f)}}, -16.0f};
    case Sfx::MenuBack: return {{{tone(Wave::Pulse25, kG5, 0.04f), tone(Wave::Pulse25, kC5, 0.07f)}}, -16.0f};
    case Sfx::GameOver:
        return {{{tone(Wave::Pulse25, kC5, 0.2f), tone(Wave::Pulse25, kG4, 0.2f), tone(Wave::Pulse25, kE4, 0.3f),
                  tone(Wave::Pulse25, kA4, 0.15f), tone(Wave::Pulse25, kB4, 0.15f), tone(Wave::Pulse25, kA4, 0.15f),
                  tone(Wave::Pulse25, kG4 + 1.0f, 0.2f), tone(Wave::Pulse25, kA4 + 1.0f, 0.2f), tone(Wave::Pulse25, kG4, 0.6f)},
                 {tone(Wave::Triangle, kC3, 0.7f), tone(Wave::Triangle, kA3, 0.45f), tone(Wave::Triangle, kC3, 1.0f)}},
                -12.0f};
    case Sfx::Count: break;
    }
    return {};
}

// One bar per string; the songs are 4 or 8 bars of 16 sixteenth-steps.
Song _song(const Tune tune)
{
    switch (tune) {
    case Tune::Grass: // C major, bouncy.
        return {.bpm = 150.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.8f,
                            .loop = "E5 - . E5 G5 - . E5 C6 - B5 - A5 - G5 - | A5 - . A5 G5 - E5 - D5 - - - . . . . | "
                                    "F5 - . F5 A5 - . F5 C6 - B5 - A5 - F5 - | G5 - - - E5 - C5 - D5 - - - . . . . | "
                                    "E5 - . E5 G5 - . E5 C6 - D6 - E6 - C6 - | A5 - - - G5 - E5 - G5 - A5 - G5 - . . | "
                                    "F5 - A5 - D6 - C6 - B5 - G5 - E5 - D5 - | C5 - - - G4 - - - C5 - - - . . . ."},
                           {.wave = Wave::Pulse12, .volume = 0.3f,
                            .loop = "C5 E5 G5 E5 C5 E5 G5 E5 C5 E5 G5 E5 C5 E5 G5 E5 | A4 C5 E5 C5 A4 C5 E5 C5 A4 C5 E5 C5 A4 C5 E5 C5 | "
                                    "F4 A4 C5 A4 F4 A4 C5 A4 F4 A4 C5 A4 F4 A4 C5 A4 | G4 B4 D5 B4 G4 B4 D5 B4 G4 B4 D5 B4 G4 B4 D5 B4"},
                           {.wave = Wave::Triangle,
                            .loop = "C3 - . C3 G2 - . G2 C3 - . C3 G2 - E2 - | A2 - . A2 E2 - . E2 A2 - . A2 E2 - C3 - | "
                                    "F2 - . F2 C3 - . C3 F2 - . F2 C3 - A2 - | G2 - . G2 D3 - . D3 G2 - B2 - D3 - G2 -"},
                           {.wave = Wave::Noise, .volume = 0.3f, .loop = "C4 . C8 . C6 . C8 . C4 . C8 C4 C6 . C8 ."}},
                .peakDb = -17.0f};
    case Tune::Desert: // A minor with a raised seventh, a swaying caravan.
        return {.bpm = 136.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.8f,
                            .loop = "A4 - C5 - E5 - . E5 F5 - E5 - D5 - C5 - | B4 - - - G#4 - B4 - A4 - - - . . . . | "
                                    "A4 - C5 - E5 - . E5 A5 - G#5 - F5 - E5 - | F5 - E5 - D5 - B4 - E5 - - - . . . . | "
                                    "C5 - D5 - E5 - . C5 D5 - E5 - F5 - E5 - | D5 - C5 - B4 - C5 - D5 - - - . . . . | "
                                    "E5 - F5 - G#5 - A5 - B5 - A5 - G#5 - E5 - | A5 - - - E5 - - - A4 - - - . . . ."},
                           {.wave = Wave::Triangle,
                            .loop = "A2 - . A2 E3 - . A2 A2 - . A2 E3 - E2 - | E2 - . E2 B2 - . E2 E2 - . E2 G#2 - B2 - | "
                                    "A2 - . A2 E3 - . A2 A2 - . A2 E3 - C3 - | D2 - . D2 A2 - . D2 E2 - . E2 G#2 - B2 -"},
                           {.wave = Wave::Noise, .volume = 0.3f, .loop = "C3 . . C8 C6 . C8 . C3 . C8 C8 C6 . C8 C8"}},
                .peakDb = -17.0f};
    case Tune::Ice: // D major, light and glassy.
        return {.bpm = 126.0f,
                .tracks = {{.wave = Wave::Pulse12, .volume = 0.75f,
                            .loop = "F#5 - - - A5 - - - D6 - C#6 - A5 - - - | B5 - - - A5 - F#5 - E5 - - - . . . . | "
                                    "G5 - - - B5 - - - E6 - D6 - B5 - - - | A5 - - - F#5 - D5 - E5 - - - . . . . | "
                                    "F#5 - - - A5 - - - D6 - E6 - F#6 - - - | G6 - F#6 - E6 - D6 - B5 - - - . . . . | "
                                    "G5 - B5 - E6 - D6 - C#6 - A5 - E5 - G5 - | F#5 - - - - - - - D5 - - - . . . ."},
                           {.wave = Wave::Pulse25, .volume = 0.25f,
                            .loop = "D5 F#5 A5 D6 A5 F#5 D5 F#5 A5 D6 A5 F#5 D5 F#5 A5 F#5 | "
                                    "G4 B4 D5 G5 D5 B4 G4 B4 D5 G5 D5 B4 G4 B4 D5 B4"},
                           {.wave = Wave::Triangle, .decay = 1.0f,
                            .loop = "D3 - - - - - - - A2 - - - - - - - | G2 - - - - - - - A2 - - - - - - -"},
                           {.wave = Wave::Noise, .volume = 0.22f, .loop = "C7 . . . C6 . . . C7 . . C7 C6 . . ."}},
                .peakDb = -17.0f};
    case Tune::Sky: // F major, airy and wide.
        return {.bpm = 132.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.75f,
                            .loop = "C5 - F5 - A5 - - - G5 - F5 - G5 - - - | A5 - C6 - F6 - - - E6 - C6 - D6 - - - | "
                                    "Bb5 - - - A5 - G5 - F5 - - - D5 - - - | C5 - - - E5 - G5 - F5 - - - . . . . | "
                                    "C5 - F5 - A5 - - - G5 - F5 - G5 - - - | A5 - C6 - F6 - - - G6 - F6 - E6 - - - | "
                                    "D6 - - - C6 - Bb5 - A5 - - - G5 - - - | F5 - - - - - - - . . . . . . . ."},
                           {.wave = Wave::Pulse12, .volume = 0.28f,
                            .loop = "F4 A4 C5 F5 C5 A4 F4 A4 C5 F5 C5 A4 F4 A4 C5 A4 | Bb4 D5 F5 Bb5 F5 D5 Bb4 D5 C5 E5 G5 C6 G5 E5 C5 E5"},
                           {.wave = Wave::Triangle,
                            .loop = "F2 - . F2 C3 - . F2 F2 - . F2 C3 - A2 - | Bb2 - . Bb2 F2 - . Bb2 C3 - . C3 G2 - E2 -"},
                           {.wave = Wave::Noise, .volume = 0.25f, .loop = "C4 . C8 C8 C6 . C8 . C4 . C8 . C6 . C8 C8"}},
                .peakDb = -17.0f};
    case Tune::Fortress: // D minor, heavy steps.
        return {.bpm = 118.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.7f,
                            .loop = "D5 - - - F5 - - - E5 - - - C#5 - - - | D5 - - - A4 - - - Bb4 - - - A4 - - - | "
                                    "D5 - - - F5 - - - G5 - - - A5 - - - | Bb5 - A5 - G5 - F5 - E5 - - - C#5 - - -"},
                           {.wave = Wave::Triangle,
                            .loop = "D2 D2 D3 D2 D2 D2 D3 D2 D2 D2 D3 D2 C#2 C#2 C#3 C#2 | "
                                    "Bb1 Bb1 Bb2 Bb1 Bb1 Bb1 Bb2 Bb1 A1 A1 A2 A1 A1 A1 C#2 E2"},
                           {.wave = Wave::Noise, .volume = 0.32f, .loop = "C2 . . . C6 . C2 . C2 . . . C6 . C8 C8"}},
                .peakDb = -17.0f};
    case Tune::Bonus: // Bb, staccato, a little sneaky.
        return {.bpm = 140.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.7f,
                            .loop = "Bb4 . Bb5 . G4 . G5 . Ab4 . Ab5 . F4 . F5 . | Bb4 . Bb5 . G4 . G5 . Ab4 . Ab5 . . . . . | "
                                    "Eb5 . D5 . Db5 . C5 . Eb5 . D5 . Ab4 . G4 . | Db5 . C5 . B4 . Bb4 . . . . . . . . ."},
                           {.wave = Wave::Triangle,
                            .loop = "Bb2 . . . G2 . . . Ab2 . . . F2 . . . | Eb2 . . . C2 . . . Db2 . . . F2 . . ."},
                           {.wave = Wave::Noise, .volume = 0.25f, .loop = "C6 . C8 . C6 . C8 . C6 . C8 . C6 C6 C8 ."}},
                .peakDb = -17.0f};
    case Tune::Map: // G major, a stroll.
        return {.bpm = 112.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.7f,
                            .loop = "G5 - - - D5 - - - G5 - A5 - B5 - - - | C6 - - - B5 - A5 - G5 - - - . . . . | "
                                    "E5 - - - G5 - - - C6 - B5 - A5 - - - | D5 - - - F#5 - A5 - G5 - - - . . . ."},
                           {.wave = Wave::Triangle,
                            .loop = "G2 - D3 - G2 - D3 - G2 - D3 - G2 - B2 - | C3 - G2 - C3 - G2 - D3 - A2 - D3 - F#2 -"},
                           {.wave = Wave::Noise, .volume = 0.2f, .loop = "C6 . . . C8 . . . C6 . . . C8 . C8 ."}},
                .peakDb = -18.0f};
    case Tune::Boss: // E minor, urgent.
        return {.bpm = 172.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.7f,
                            .loop = "E5 - E5 - G5 - E5 - B5 - A5 - G5 - F#5 - | E5 - - - B4 - - - C5 - D5 - - - . . | "
                                    "C5 - C5 - E5 - C5 - G5 - F#5 - E5 - D5 - | B4 - - - D#5 - - - F#5 - - - B5 - - -"},
                           {.wave = Wave::Triangle,
                            .loop = "E2 E2 E3 E2 E2 E2 E3 E2 E2 E2 E3 E2 D2 D2 D3 D2 | C2 C2 C3 C2 C2 C2 C3 C2 B1 B1 B2 B1 B1 B1 B2 D#2"},
                           {.wave = Wave::Noise, .volume = 0.33f, .loop = "C2 C8 C6 C8 C2 C8 C6 C8 C2 C8 C6 C8 C2 C2 C6 C6"}},
                .peakDb = -17.0f};
    case Tune::Title: // C major, a jaunty opening.
        return {.bpm = 140.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.8f,
                            .loop = "C5 - C5 - E5 - G5 - C6 - - - G5 - - - | A5 - G5 - F5 - E5 - D5 - - - . . . . | "
                                    "F5 - F5 - A5 - C6 - F6 - - - C6 - - - | B5 - A5 - G5 - F5 - G5 - - - . . . . | "
                                    "C6 - B5 - A5 - G5 - A5 - G5 - F5 - E5 - | F5 - E5 - D5 - C5 - D5 - - - G4 - - - | "
                                    "C5 - E5 - G5 - C6 - E6 - - - D6 - - - | C6 - - - G5 - - - C6 - - - . . . ."},
                           {.wave = Wave::Pulse12, .volume = 0.28f,
                            .loop = "E4 G4 C5 G4 E4 G4 C5 G4 F4 A4 C5 A4 G4 B4 D5 B4 | A4 C5 F5 C5 A4 C5 F5 C5 G4 B4 D5 B4 G4 B4 D5 B4"},
                           {.wave = Wave::Triangle,
                            .loop = "C3 - G2 - C3 - G2 - F2 - C3 - G2 - D3 - | F2 - C3 - F2 - C3 - G2 - D3 - G2 - B2 -"},
                           {.wave = Wave::Noise, .volume = 0.3f, .loop = "C4 . C8 . C6 . C8 C8 C4 . C8 . C6 . C8 ."}},
                .peakDb = -17.0f};
    case Tune::Ending: // F major, slow and warm.
        return {.bpm = 96.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.7f,
                            .loop = "A5 - - - G5 - - - F5 - - - C5 - - - | D5 - - - F5 - - - A5 - - - G5 - - - | "
                                    "Bb5 - - - A5 - - - G5 - - - F5 - - - | E5 - - - G5 - - - F5 - - - . . . ."},
                           {.wave = Wave::Pulse12, .volume = 0.3f, .decay = 1.0f,
                            .loop = "F4 - A4 - C5 - A4 - D4 - F4 - A4 - F4 - | Bb3 - D4 - F4 - D4 - C4 - E4 - G4 - E4 -"},
                           {.wave = Wave::Triangle, .decay = 0.5f,
                            .loop = "F2 - - - - - - - D2 - - - - - - - | Bb1 - - - - - - - C2 - - - - - - -"},
                           {.wave = Wave::Noise, .volume = 0.18f, .loop = "C7 . . . . . . . C7 . . . C7 . . ."}},
                .peakDb = -18.0f};
    case Tune::Star: // C major, breathless.
        return {.bpm = 184.0f,
                .tracks = {{.wave = Wave::Pulse25, .volume = 0.75f,
                            .loop = "C5 . C5 . C5 . D5 C5 . C5 . D5 C5 . C5 . | B4 . B4 . B4 . C5 B4 . B4 . C5 B4 . B4 ."},
                           {.wave = Wave::Triangle,
                            .loop = "C3 . G3 . C3 . G3 . C3 . G3 . C3 . G3 . | G2 . D3 . G2 . D3 . G2 . D3 . G2 . D3 ."},
                           {.wave = Wave::Noise, .volume = 0.3f, .loop = "C4 C8 C6 C8 C4 C8 C6 C8 C4 C8 C6 C8 C4 C4 C6 C8"}},
                .peakDb = -17.0f};
    case Tune::Count: break;
    }
    return {};
}

} // namespace

std::vector<float> renderSfx(const Sfx sfx, const std::uint32_t sampleRate)
{
    const Recipe recipe = _recipe(sfx);
    return gamelib::audio::renderMix(recipe.tracks, recipe.peakDb, sampleRate);
}

Song song(const Tune tune, const bool hurry)
{
    Song s = _song(tune);
    if (hurry) {
        s.bpm *= 1.25f; // the hurry-up take: the same song a quarter faster
    }
    return s;
}

Music::Music(gamelib::audio::Player& player)
{
    for (std::size_t i = 0; i < kTuneCount; ++i) {
        const gamelib::audio::RenderedSong rendered = gamelib::audio::renderSong(song(static_cast<Tune>(i), false), player.sampleRate());
        m_tracks[i] = player.addMusic(rendered.samples, rendered.loopStart);
    }
    for (std::size_t i = 0; i < kHurryTunes; ++i) {
        const gamelib::audio::RenderedSong rendered = gamelib::audio::renderSong(song(static_cast<Tune>(i), true), player.sampleRate());
        m_hurry[i] = player.addMusic(rendered.samples, rendered.loopStart);
    }
}

void Music::play(gamelib::audio::Player& player, const Tune tune, const bool hurry, const float fadeSeconds) const
{
    const auto index = static_cast<std::size_t>(tune);
    const gamelib::audio::SoundId track = hurry && index < kHurryTunes ? m_hurry[index] : m_tracks[index < kTuneCount ? index : 0];
    if (player.music() != track) {
        player.playMusic(track, fadeSeconds);
    }
}

void Music::stop(gamelib::audio::Player& player) const
{
    player.stopMusic(0.3f);
}

} // namespace bounder::app
