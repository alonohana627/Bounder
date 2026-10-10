#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// The near parallax layer along the bottom of the screen
// (draw::drawParallax): params.xy scroll (canvas units), params.z theme.
// Grass: round bushes; desert: cacti; ice: snowy pines; sky: cloud puffs;
// fortress and castle: pillars. color = body, color2 = highlight. Each
// shape sits in its own 260-unit cell, so nothing crosses a cell edge.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

// A 0..1 hash of a point's whole-unit cell (GameLib's hash12 takes integers).
float hashf(vec2 p)
{
    return hash12(uvec2(ivec2(floor(p))));
}

const vec3 kInk = vec3(0.05, 0.03, 0.05);

void main()
{
    vec2 world = inUv * sprite.size + sprite.params.xy;
    int theme = int(sprite.params.z + 0.5);
    float cellW = 260.0;
    float cell = floor(world.x / cellW);
    float h = hashf(vec2(cell, 3.0));
    vec2 p = vec2(mod(world.x, cellW) - cellW * 0.5, world.y - sprite.size.y);
    float d = 1e5;
    if (h < 0.35 && theme != 4 && theme != 5) {
        outColor = vec4(0.0);
        return;
    }
    float scale = 0.75 + 0.5 * hashf(vec2(cell, 7.0));
    p /= scale;
    if (theme == 0) {
        d = min(min(sdCircle(p - vec2(-40.0, -30.0), 42.0), sdCircle(p - vec2(15.0, -50.0), 52.0)), sdCircle(p - vec2(65.0, -25.0), 36.0));
        d = max(d, p.y);
    } else if (theme == 1) {
        d = sdRoundBox(p - vec2(0.0, -70.0), vec2(16.0, 70.0), 15.0);
        d = min(d, sdRoundBox(p - vec2(-32.0, -86.0), vec2(10.0, 26.0), 9.0));
        d = min(d, sdRoundBox(p - vec2(-22.0, -62.0), vec2(16.0, 8.0), 7.0));
        d = min(d, sdRoundBox(p - vec2(30.0, -104.0), vec2(10.0, 22.0), 9.0));
        d = min(d, sdRoundBox(p - vec2(22.0, -84.0), vec2(14.0, 8.0), 7.0));
    } else if (theme == 2) {
        for (int i = 0; i < 3; ++i) {
            float w = 70.0 - float(i) * 18.0;
            float y = -30.0 - float(i) * 42.0;
            d = min(d, max(abs(p.x) * 1.1 + (p.y - y) * 0.9 - w, y - 34.0 - p.y));
        }
        d = min(d, sdBox(p - vec2(0.0, -14.0), vec2(9.0, 14.0)));
    } else if (theme == 3) {
        d = min(min(sdCircle(p - vec2(-50.0, -10.0), 40.0), sdCircle(p - vec2(0.0, -30.0), 55.0)), sdCircle(p - vec2(55.0, -8.0), 38.0));
    } else if (theme == 4 || theme == 5) {
        p = vec2(mod(world.x, 520.0) - 260.0, world.y - sprite.size.y);
        d = sdBox(p - vec2(0.0, -200.0), vec2(30.0, 200.0));
        d = min(d, sdBox(p - vec2(0.0, -400.0), vec2(42.0, 14.0)));
    } else {
        outColor = vec4(0.0);
        return;
    }
    d *= scale;
    float cover = coverage(d);
    float edge = coverage(d + 3.0);
    vec3 rgb = sprite.color.rgb;
    // Light from above-left.
    float light = smoothstep(-40.0, 20.0, -p.y * 0.4 - p.x * 0.3 + 30.0 * valueNoise(world * 0.02));
    rgb = mix(rgb, sprite.color2.rgb, light * 0.45);
    vec3 outline = mix(kInk, sprite.color.rgb * 0.5, 0.6);
    vec3 c = mix(outline, rgb, edge);
    outColor = vec4(dither(c, gl_FragCoord.xy, 0.02), cover * sprite.color.a);
}
