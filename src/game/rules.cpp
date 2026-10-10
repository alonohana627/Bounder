#include "rules.hpp"

#include <algorithm>
#include <cmath>

namespace bounder::game {

std::string encodeInput(const Input& input)
{
    std::string text = "....";
    text[0] = input.move < 0 ? 'L' : (input.move > 0 ? 'R' : '.');
    text[1] = input.down ? 'd' : '.';
    text[2] = input.jumpPressed ? 'p' : (input.jumpHeld ? 'h' : '.');
    text[3] = input.runPressed ? 'f' : (input.runHeld ? 'r' : '.');
    return text;
}

std::optional<Input> decodeInput(const std::string_view text)
{
    if (text.size() != 4) {
        return std::nullopt;
    }
    Input input;
    switch (text[0]) {
    case 'L': input.move = -1; break;
    case 'R': input.move = 1; break;
    case '.': break;
    default: return std::nullopt;
    }
    switch (text[1]) {
    case 'd': input.down = true; break;
    case '.': break;
    default: return std::nullopt;
    }
    switch (text[2]) {
    case 'p': input.jumpPressed = true; input.jumpHeld = true; break;
    case 'h': input.jumpHeld = true; break;
    case '.': break;
    default: return std::nullopt;
    }
    switch (text[3]) {
    case 'f': input.runPressed = true; input.runHeld = true; break;
    case 'r': input.runHeld = true; break;
    case '.': break;
    default: return std::nullopt;
    }
    return input;
}

gamelib::core::PlatformerTuning tuningFor(const bool running, const float speedX, const bool downhill, const bool icy)
{
    gamelib::core::PlatformerTuning t;
    // Walking: about 5.6 tiles a second; running: about 9.75. Speed builds
    // over half a second (momentum), turning skids.
    t.maxSpeed = running ? kRunSpeed : kWalkSpeed;
    if (downhill) {
        t.maxSpeed *= 1.3f; // running down a slope is faster
    }
    t.groundAccel = running ? 0.075f : 0.06f;
    t.groundDecel = 0.12f;
    if (icy) {
        // Ice: slow to stop and to turn round.
        t.groundDecel = 0.05f;
    }
    t.airAccel = 0.065f;
    t.airDecel = 0.03f; // momentum carries through the air
    // A standing jump rises 4.25 tiles and peaks after 26 ticks; at full
    // running speed the same press rises 5.25 tiles. Falls are heavier.
    t.setJumpArc(4.25f * kTile, 26);
    const float runShare = std::clamp((std::abs(speedX) - kWalkSpeed * 0.5f) / (kRunSpeed - kWalkSpeed * 0.5f), 0.0f, 1.0f);
    t.jumpSpeed = gamelib::core::PlatformerTuning::jumpSpeedFor((4.25f + runShare) * kTile, t.gravity);
    t.fallGravity = 1.5f;
    t.maxFallSpeed = 4.5f;
    t.jumpCut = 0.4f;
    t.apexThreshold = 0.6f;
    t.apexGravity = 0.6f;
    t.coyoteTicks = 6;
    t.jumpBufferTicks = 6;
    t.cornerCorrection = 4.0f;
    t.hazardInset = 3.0f;
    return t;
}

gamelib::core::Vec2 heroSize(const Power power)
{
    return power == Power::Small ? kSmallSize : kBigSize;
}

} // namespace bounder::game
