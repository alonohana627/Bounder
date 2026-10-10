#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/noise.glsl"

// The far silhouette behind a level, a full-canvas parallax layer
// (draw::drawParallax): params.xy scroll (canvas units), params.z theme
// (0 grass: rolling hills, 1 desert: mesas, 2 ice: peaks with a snow line,
// 3 sky: towering cloud banks), params.w the base line as a fraction of the
// canvas height. color = fill, color2 = the lit top band (snow on the peaks).
// A heightfield in world space, so it never swims as the camera moves.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 world = inUv * sprite.size + sprite.params.xy;
    int theme = int(sprite.params.z + 0.5);
    float base = sprite.params.w * sprite.size.y;
    float x = mod(world.x, 24000.0); // wrapped: no precision stripes far from the origin
    float h;
    if (theme == 1) {
        // Mesas: plateaus with steep sides, a little jitter on top.
        float plateau = smoothstep(0.42, 0.58, valueNoise(vec2(x / 520.0, 3.1)));
        h = base - 40.0 - plateau * 170.0 + (valueNoise(vec2(x / 37.0, 9.0)) - 0.5) * 8.0;
    } else if (theme == 2) {
        // Peaks: ridged noise in three octaves.
        float r = 0.0;
        float amp = 1.0;
        float f = 1.0 / 420.0;
        for (int i = 0; i < 3; ++i) {
            r += amp * (1.0 - abs(valueNoise(vec2(x * f, float(i) * 7.3)) * 2.0 - 1.0));
            amp *= 0.45;
            f *= 2.3;
        }
        h = base - r * 190.0;
    } else if (theme == 3) {
        // Cloud banks: rounded bumps.
        h = base - 60.0 - valueNoise(vec2(x / 260.0, 1.7)) * 120.0 - valueNoise(vec2(x / 90.0, 4.2)) * 30.0;
    } else {
        // Rolling hills.
        h = base - 30.0 - valueNoise(vec2(x / 380.0, 2.3)) * 110.0 - valueNoise(vec2(x / 140.0, 5.9)) * 30.0;
    }
    float d = h - world.y; // > 0 above the silhouette
    float cover = clamp(0.5 - d / max(fwidth(d), 1e-4), 0.0, 1.0);
    vec3 rgb = sprite.color.rgb;
    if (theme == 2) {
        // Snow above a wavy line on every peak.
        float snowLine = base - 120.0 + (valueNoise(vec2(x / 60.0, 3.3)) - 0.5) * 40.0;
        rgb = mix(rgb, sprite.color2.rgb, step(world.y, snowLine));
    } else {
        // A lighter band along the top edge.
        rgb = mix(sprite.color2.rgb, rgb, smoothstep(0.0, 26.0, world.y - h));
    }
    outColor = vec4(dither(rgb, gl_FragCoord.xy, 0.02), sprite.color.a * cover);
}
