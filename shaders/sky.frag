#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// The backdrop behind a level, a full-canvas parallax layer
// (draw::drawParallax): params.xy scroll (canvas units), params.z theme
// (0 grass, 1 desert, 2 ice, 3 sky, 4 fortress, 5 castle, 6 bonus room).
// Outdoors: a sky gradient from color (top) to color2 (horizon), a sun, and
// snowfall on ice. Indoors: a wall of big stones with glowing windows.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

// A 0..1 hash of a point's whole-unit cell (GameLib's hash12 takes integers).
float hashf(vec2 p)
{
    return hash12(uvec2(ivec2(floor(p))));
}

void main()
{
    vec2 canvasPos = inUv * sprite.size;
    vec2 world = canvasPos + sprite.params.xy;
    int theme = int(sprite.params.z + 0.5);
    vec3 rgb;
    if (theme <= 3) {
        float t = smoothstep(0.0, 1.0, inUv.y);
        rgb = mix(sprite.color.rgb, sprite.color2.rgb, t);
        if (theme != 2) {
            vec2 sun = vec2(sprite.size.x * 0.8, sprite.size.y * 0.2);
            float d = length(canvasPos - sun);
            float r = theme == 1 ? 110.0 : 80.0;
            vec3 sunColor = theme == 1 ? vec3(1.0, 0.86, 0.45) : vec3(1.0, 0.94, 0.62);
            rgb = mix(rgb, sunColor, (1.0 - smoothstep(r - 2.0, r + 2.0, d)) * 0.85);
            rgb += sunColor * 0.25 * exp(-d / (r * 2.2)) * (theme == 3 ? 0.5 : 1.0);
        } else {
            // Snowfall in two depths.
            for (int layer = 0; layer < 2; ++layer) {
                float scale = layer == 0 ? 60.0 : 34.0;
                vec2 drift = vec2(sin(sprite.time * 0.6 + float(layer)) * 20.0, sprite.time * (layer == 0 ? 60.0 : 35.0));
                vec2 q = (world * (layer == 0 ? 1.0 : 0.6) - drift) / scale;
                vec2 cell = floor(q);
                vec2 f = fract(q) - 0.5;
                vec2 offset = vec2(hashf(cell), hashf(cell + 9.1)) - 0.5;
                float flake = 1.0 - smoothstep(0.03, 0.07, length(f - offset * 0.6));
                rgb = mix(rgb, vec3(1.0), flake * (layer == 0 ? 0.9 : 0.5));
            }
        }
        if (theme == 3) {
            // High up: faint stars near the top.
            vec2 cell = floor(canvasPos / 40.0);
            float star = step(0.93, hashf(cell)) * (1.0 - smoothstep(0.0, 0.45, inUv.y));
            vec2 f = fract(canvasPos / 40.0) - 0.5;
            rgb += vec3(1.0) * star * (1.0 - smoothstep(0.02, 0.06, length(f))) * (0.6 + 0.4 * sin(sprite.time * 3.0 + cell.x));
        }
    } else {
        // Indoors: big stone blocks, darker at the top, with lit windows.
        vec2 block = vec2(120.0, 64.0);
        float row = floor(world.y / block.y);
        vec2 inBlock = vec2(mod(world.x + mod(row, 2.0) * block.x * 0.5, block.x), mod(world.y, block.y));
        float mortar = min(min(inBlock.x, block.x - inBlock.x), min(inBlock.y, block.y - inBlock.y));
        float shade = 0.85 + 0.25 * hashf(vec2(floor((world.x + mod(row, 2.0) * block.x * 0.5) / block.x), row));
        rgb = sprite.color.rgb * shade;
        rgb = mix(sprite.color2.rgb * 0.5, rgb, smoothstep(1.0, 4.0, mortar));
        rgb *= mix(0.6, 1.0, inUv.y);
        if (theme != 6) {
            // Arched windows with a warm flicker.
            vec2 win = vec2(mod(world.x, 480.0) - 240.0, world.y - 300.0);
            float d = min(sdBox(win - vec2(0.0, 30.0), vec2(34.0, 50.0)), sdCircle(win - vec2(0.0, -20.0), 34.0));
            float flicker = 0.85 + 0.15 * sin(sprite.time * 9.0 + floor(world.x / 480.0) * 2.0);
            rgb = mix(rgb, sprite.color2.rgb * 1.6 * flicker, (1.0 - smoothstep(-1.5, 1.5, d)) * 0.8);
            rgb += sprite.color2.rgb * 0.25 * exp(-max(d, 0.0) / 60.0) * flicker;
        }
    }
    outColor = vec4(dither(rgb, gl_FragCoord.xy, 0.02), 1.0);
}
