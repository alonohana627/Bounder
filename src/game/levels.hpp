#pragma once

// The campaign's content: 4 worlds of 4 levels and a fortress, then the
// final castle - 21 levels written as text, one character per 16-unit tile.
// A level is a list of segments (blocks of rows of the same width) joined
// left to right, so each part can be read and edited on its own. '.' is
// empty space. The characters are the tile ids (core::charTile):
//
//   tiles      #  ground (grass, sand, snow or cloud by theme; stone in castles)
//              X  hard block      =  one-way platform     B  brick
//              ?  coin block      P  power-up block       *  star block
//              C  multi-coin brick h  hidden coin block   H  hidden 1-up block
//              u  used block      o  coin
//              /  \  45-degree slopes rising right / left
//              a A  22.5-degree slope rising right (low, high half)
//              D d  22.5-degree slope falling right (high, low half)
//              { }  pipe top (left, right)   [ ]  pipe body
//              E    an enterable pipe's top-left: the k-th E of the level
//                   (left to right) leads to bonus room k, whose E leads back
//              O    a pipe top-left the hero comes out of: bonus room k's O
//                   is where he arrives; the level's k-th O is where he returns
//              ^  spikes         ~  lava
//              -  |  rails: a moving platform's track (see m)
//   markers    S  start   F  checkpoint flag   G  goal pole (its foot)
//              1 2 3  star coins     Z  boss     z  the boss arena's gate column
//              m  a moving platform (3 wide, its left end); it rides the
//                 rail ('-' across or '|' up and down) through its cell
//              w  a falling platform (3 wide, its left end)
//              g  walker  k  shell walker  r  red shell (turns at ledges)
//              j  hopper  f  flier (sine)  y  flier (up and down)
//              p  plant (in the pipe right below)  s  spiky (cannot be stomped)

#include <gamelib/core/grid.hpp>
#include <gamelib/core/math.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bounder::game {

enum class Theme : std::uint8_t { Grass, Desert, Ice, Sky, Fortress, Castle, Bonus };
inline constexpr int kThemeCount = 7;

inline constexpr int kWorlds = 4;
inline constexpr int kLevelsPerWorld = 5;                      // 4 levels and a fortress
inline constexpr int kCastleLevel = kWorlds * kLevelsPerWorld; // the final castle: 20
inline constexpr int kLevelCount = kCastleLevel + 1;

inline constexpr std::string_view kMarkerSymbols = "SFG123Zzwmgkrjfyps";

struct AreaDef {
    Theme theme = Theme::Grass;
    std::vector<std::string> rows; // the joined segments, '.' already turned into ' '
};

struct LevelDef {
    std::string_view id;   // campaign id ("1-1", "1-F", "castle")
    std::string_view name;
    Theme theme = Theme::Grass;
    int time = 300;        // timer units (kTimeUnitTicks each)
    std::vector<AreaDef> areas; // area 0 is the level; the rest are bonus rooms
};

[[nodiscard]] const std::vector<LevelDef>& levels();
[[nodiscard]] const LevelDef& level(int index);

[[nodiscard]] constexpr int worldOf(const int level) { return level >= kCastleLevel ? kWorlds - 1 : level / kLevelsPerWorld; }
[[nodiscard]] constexpr bool isFortress(const int level) { return level < kCastleLevel && level % kLevelsPerWorld == 4; }
[[nodiscard]] constexpr bool hasBoss(const int level) { return isFortress(level) || level == kCastleLevel; }

// The world's name for banners ("GRASSLAND" ...).
[[nodiscard]] std::string_view worldName(int world);
[[nodiscard]] Theme worldTheme(int world);

// Joins segments (each a list of rows of one width) into an area's rows,
// '.' becoming ' '. Throws std::logic_error naming the level and segment
// when a segment's rows differ in width or segments differ in height.
[[nodiscard]] AreaDef joinSegments(std::string_view levelId, Theme theme,
                                   const std::vector<std::vector<std::string_view>>& segments);

} // namespace bounder::game
