#pragma once

// Bounder's colors: a bright cartoon palette per theme (grassland, desert,
// ice, sky, fortress, castle, bonus room) as GameLib core::Palette by role,
// so a world swaps its whole look at once; the hero, items and enemies keep
// one set of colors everywhere so they always read the same.

#include "levels.hpp"

#include <gamelib/core/color.hpp>

#include <cstddef>

namespace bounder::app {

using gamelib::core::Color;
using gamelib::core::srgb;

enum class Role : std::uint8_t {
    SkyTop,
    SkyLow,
    Hills,
    HillsRidge,
    Cloud,
    CloudShade,
    Bush,
    BushLight,
    Ground,
    GroundCap,
    Block,
    Brick,
    Pipe,
    Platform,
    PlatformTop,
    Accent, // banners, the HUD's level name
    MapGround,
    MapDetail,
    Count
};
inline constexpr std::size_t kRoleCount = static_cast<std::size_t>(Role::Count);
using ThemePalette = gamelib::core::Palette<Role, kRoleCount>;

[[nodiscard]] const ThemePalette& themePalette(game::Theme theme);

// The tile shader's theme variant for a theme (0 grass .. 6 bonus).
[[nodiscard]] constexpr float themeIndex(const game::Theme theme) { return static_cast<float>(theme); }

[[nodiscard]] inline Color glow(const Color c, const float strength)
{
    return {c.r * strength, c.g * strength, c.b * strength, c.a};
}

// Fixed colors.
inline const Color kQuestion = srgb(0xFFC93C);
inline const Color kQuestionMark = srgb(0xFFFFFF);
inline const Color kUsed = srgb(0x9C6B45);
inline const Color kCoin = srgb(0xFFD23F);
inline const Color kCoinLight = srgb(0xFFF1A6);
inline const Color kHeroCap = srgb(0xE8343B);
inline const Color kHeroOveralls = srgb(0x2F5BD8);
inline const Color kFireCap = srgb(0xFFFFFF);
inline const Color kFireOveralls = srgb(0xE8343B);
inline const Color kMushroom = srgb(0xE8343B);
inline const Color kOneUp = srgb(0x35B54A);
inline const Color kFlower = srgb(0xFF7A1A);
inline const Color kFlowerInner = srgb(0xFFE14D);
inline const Color kStar = srgb(0xFFE14D);
inline const Color kStarGlow = srgb(0xFFFFFF);
inline const Color kFire = srgb(0xFF7A1A);
inline const Color kFireCore = srgb(0xFFF07A);
inline const Color kText = srgb(0xFFFFFF);
inline const Color kTextDim = srgb(0xD7DEF0);
inline const Color kInk = srgb(0x120A14);
inline const Color kPanel = srgb(0x1A1430);

} // namespace bounder::app
