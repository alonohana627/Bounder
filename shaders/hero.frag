#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// The hero, a round little fellow in a cap and overalls, drawn from SDF
// parts with cartoon outlines. The quad is the drawn body plus a margin.
//   color    cap and shirt          color2   overalls
//   params   x facing (-1, 1), y pose (0 stand, 1 walk, 2 rise, 3 fall,
//            4 skid, 5 dead, 6 throw), z walk phase 0..1, w margin
//   bits.x   0 small, 1 big; bit 8: star power (colors cycle)

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.05, 0.03, 0.05);
const vec3 kSkin = vec3(1.0, 0.72, 0.52);
const vec3 kShoe = vec3(0.32, 0.17, 0.08);
const float kPi = 3.14159265;

vec4 acc = vec4(0.0);

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 0.85)), acc);
    acc = over(vec4(rgb, coverage(d)), acc);
}

void fill(float d, vec3 rgb)
{
    acc = over(vec4(rgb, coverage(d)), acc);
}

vec3 hue(float h)
{
    return clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
}

void main()
{
    vec2 p = spriteLocal(inUv);
    float margin = sprite.params.w;
    float height = sprite.size.y - 2.0 * margin;
    float facing = sprite.params.x < 0.0 ? -1.0 : 1.0;
    int pose = int(sprite.params.y + 0.5);
    float phase = sprite.params.z;
    bool big = (sprite.bits.x & 0xffu) == 1u;
    bool star = (sprite.bits.x & 0x100u) != 0u;

    // Feet at y = 0, up is negative; x mirrored so the hero faces +x.
    vec2 q = vec2(p.x * facing, p.y - height * 0.5);

    vec3 cap = sprite.color.rgb;
    vec3 overalls = sprite.color2.rgb;
    if (star) {
        cap = mix(cap, hue(sprite.time * 3.0), 0.75);
        overalls = mix(overalls, hue(sprite.time * 3.0 + 0.5), 0.75);
    }

    float swing = pose == 1 ? sin(phase * 2.0 * kPi) * 2.4 : 0.0;
    float lift = pose == 1 ? max(0.0, -cos(phase * 2.0 * kPi)) * 0.8 : 0.0;
    if (pose == 5) {
        q.x = p.x; // dead: facing the player
    }

    // Body proportions.
    float bodyH = big ? 13.0 : 4.6;
    float headY = big ? -21.0 : -10.0;
    float headR = big ? 5.4 : 5.2;
    if (pose == 4) {
        q.x += (q.y / height) * 2.0; // skid: leaning back
    }

    // Back foot, back arm.
    vec2 backFoot = vec2(-2.4 - swing, -1.4 - lift * 0.0);
    vec2 frontFoot = vec2(2.6 + swing, -1.4 - lift);
    if (pose == 2 || pose == 3) {
        backFoot = vec2(-3.0, -2.6);
        frontFoot = vec2(3.4, -3.6);
    }
    part(sdEllipse(q - backFoot, vec2(2.9, 1.7)), kShoe * 0.85);
    if (big) {
        vec2 arm = vec2(-4.6 + swing * 0.5, -12.5);
        part(sdCircle(q - arm, 2.1), cap * 0.85);
        fill(sdCircle(q - arm - vec2(0.0, 2.2), 1.6), kSkin * 0.9);
    }

    // Overalls (and the shirt above them on the big hero).
    if (big) {
        part(sdRoundBox(q - vec2(0.0, -15.5), vec2(4.6, 3.6), 2.2), cap);
        part(sdRoundBox(q - vec2(0.0, -7.6), vec2(4.6, 5.8), 2.4), overalls);
        // Straps and buttons.
        fill(sdSegment(q, vec2(-2.4, -13.0), vec2(-2.0, -18.5), 0.9), overalls);
        fill(sdSegment(q, vec2(2.4, -13.0), vec2(2.0, -18.5), 0.9), overalls);
        fill(sdCircle(q - vec2(-2.2, -12.6), 0.8), vec3(1.0, 0.85, 0.2));
        fill(sdCircle(q - vec2(2.2, -12.6), 0.8), vec3(1.0, 0.85, 0.2));
    } else {
        part(sdRoundBox(q - vec2(0.0, -2.0 - bodyH * 0.5), vec2(3.8, bodyH * 0.5 + 0.4), 2.0), overalls);
    }

    // Front foot.
    part(sdEllipse(q - frontFoot, vec2(3.0, 1.8)), kShoe);

    // Head: face, eye, nose, cap with a brim.
    vec2 h = q - vec2(0.4, headY);
    part(sdCircle(h, headR), kSkin);
    // The cap: the head's top, cut off above the eyes.
    float capD = max(sdCircle(h - vec2(-0.3, -0.6), headR + 0.4), h.y + 1.0 - h.x * 0.15);
    part(capD, cap);
    part(sdEllipse(h - vec2(4.0, -1.2), vec2(3.2, 1.1)), cap * 0.9);
    // A letter-like badge on the cap.
    fill(sdCircle(h - vec2(0.2, -3.3), 1.2), vec3(1.0));
    if (pose == 5) {
        // Dead: crossed eyes.
        fill(sdSegment(h, vec2(0.4, -0.2), vec2(2.4, 1.8), 0.45), kInk);
        fill(sdSegment(h, vec2(0.4, 1.8), vec2(2.4, -0.2), 0.45), kInk);
        fill(sdSegment(h, vec2(-2.4, -0.2), vec2(-0.4, 1.8), 0.45), kInk);
        fill(sdSegment(h, vec2(-2.4, 1.8), vec2(-0.4, -0.2), 0.45), kInk);
    } else {
        fill(sdEllipse(h - vec2(2.2, 0.8), vec2(1.15, 1.7)), vec3(1.0));
        fill(sdEllipse(h - vec2(2.6, 1.0), vec2(0.6, 1.0)), kInk);
        part(sdCircle(h - vec2(4.7, 2.6), 1.35), kSkin * 1.02);
        // Moustache.
        fill(sdEllipse(h - vec2(3.4, 3.9), vec2(2.0, 0.75)), kShoe * 0.7);
    }
    if (big) {
        vec2 arm = vec2(4.2 - swing * 0.5, -12.0);
        if (pose == 6) {
            arm = vec2(6.2, -14.0); // throwing: the arm out front
        }
        part(sdCircle(q - arm, 2.1), cap * 0.95);
        part(sdCircle(q - arm - vec2(0.6, 2.0), 1.6), kSkin);
    }
    outColor = acc;
}
