#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// One cell of the overworld map, by kind (params.x):
//   0 ground (theme color, a soft checker)   1 tree   2 rock   3 water
//   4 path (params.y: 1 open, 0 still closed; bits.x: which sides it joins,
//     1 up 2 right 4 down 8 left)
//   5 level node (params.y: 1 cleared)   6 fortress   7 castle
//   8 gate (params.z: -1 back, 1 on)   9 start
// color = main, color2 = accent. Cells are drawn in canvas units.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

// A 0..1 hash of a point's whole-unit cell (GameLib's hash12 takes integers).
float hashf(vec2 p)
{
    return hash12(uvec2(ivec2(floor(p))));
}

const vec3 kInk = vec3(0.05, 0.03, 0.05);

vec4 acc = vec4(0.0);

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 2.0)), acc);
    acc = over(vec4(rgb, coverage(d)), acc);
}

void fill(float d, vec3 rgb)
{
    acc = over(vec4(rgb, coverage(d)), acc);
}

void main()
{
    vec2 p = spriteLocal(inUv);
    float s = sprite.size.x * 0.5; // half a cell
    int kind = int(sprite.params.x + 0.5);
    vec3 base = sprite.color.rgb;
    vec3 accent = sprite.color2.rgb;
    vec2 world = sprite.position + inUv * sprite.size;
    if (kind == 0) {
        float checker = mod(floor(world.x / (2.0 * s)) + floor(world.y / (2.0 * s)), 2.0);
        vec3 c = base * (0.95 + 0.05 * checker) * (0.94 + 0.12 * valueNoise(world * 0.03));
        float tuft = step(0.86, hashf(floor(world / 9.0)));
        c = mix(c, accent, tuft * 0.5);
        outColor = vec4(dither(c, gl_FragCoord.xy, 0.02), 1.0);
        return;
    }
    if (kind == 1) {
        part(sdBox(p - vec2(0.0, s * 0.45), vec2(s * 0.12, s * 0.3)), vec3(0.45, 0.28, 0.14));
        part(sdCircle(p - vec2(0.0, -s * 0.15), s * 0.58), base);
        fill(sdCircle(p - vec2(-s * 0.18, -s * 0.33), s * 0.22), accent);
    } else if (kind == 2) {
        part(sdRoundBox(p - vec2(0.0, s * 0.15), vec2(s * 0.62, s * 0.45), s * 0.3), base);
        fill(sdRoundBox(p - vec2(-s * 0.15, -s * 0.05), vec2(s * 0.3, s * 0.15), s * 0.12), accent);
    } else if (kind == 3) {
        float wave = sin(world.x * 0.08 + sprite.time * 2.0) * sin(world.y * 0.1 - sprite.time * 1.3);
        vec3 c = mix(base, accent, 0.5 + 0.5 * wave);
        outColor = vec4(c, 1.0);
        return;
    } else if (kind == 4) {
        uint sides = sprite.bits.x;
        bool open = sprite.params.y > 0.5;
        float d = sdCircle(p, s * 0.18);
        if ((sides & 1u) != 0u) d = min(d, sdSegment(p, vec2(0.0), vec2(0.0, -s), s * 0.18));
        if ((sides & 2u) != 0u) d = min(d, sdSegment(p, vec2(0.0), vec2(s, 0.0), s * 0.18));
        if ((sides & 4u) != 0u) d = min(d, sdSegment(p, vec2(0.0), vec2(0.0, s), s * 0.18));
        if ((sides & 8u) != 0u) d = min(d, sdSegment(p, vec2(0.0), vec2(-s, 0.0), s * 0.18));
        if (open) {
            acc = over(vec4(base * 0.55, coverage(d - 3.0)), acc);
            acc = over(vec4(base, coverage(d)), acc);
            // Dots along the way.
            vec2 dots = abs(fract(world / (s * 0.66)) - 0.5);
            fill(max(d, sdCircle(dots * s * 0.66, s * 0.07)), accent);
        } else {
            acc = over(vec4(base * 0.5, coverage(d) * 0.35), acc);
        }
    } else if (kind == 5) {
        bool cleared = sprite.params.y > 0.5;
        part(sdRoundBox(p, vec2(s * 0.62), s * 0.22), cleared ? accent : base);
        fill(sdRoundBox(p - vec2(0.0, -s * 0.3), vec2(s * 0.5, s * 0.08), s * 0.06), vec3(1.0) * 0.35 + (cleared ? accent : base) * 0.65);
    } else if (kind == 6 || kind == 7) {
        float w = kind == 7 ? 0.8 : 0.55;
        part(sdBox(p - vec2(0.0, s * 0.2), vec2(s * w, s * 0.6)), base);
        float crenel = abs(mod(p.x + s * 0.1, s * 0.4) - s * 0.2) - s * 0.1;
        part(max(sdBox(p - vec2(0.0, -s * 0.45), vec2(s * w, s * 0.12)), crenel), base * 0.9);
        part(min(sdBox(p - vec2(0.0, s * 0.5), vec2(s * 0.18, s * 0.3)), sdCircle(p - vec2(0.0, s * 0.22), s * 0.18)), kInk * 2.0);
        fill(sdSegment(p, vec2(0.0, -s * 0.6), vec2(0.0, -s * 1.0), s * 0.04), kInk);
        vec2 v[kMaxPolygonVertices];
        v[0] = vec2(0.0, -s * 1.0);
        v[1] = vec2(s * 0.45, -s * 0.88);
        v[2] = vec2(0.0, -s * 0.76);
        part(sdPolygon(p, v, 3), accent);
    } else if (kind == 8) {
        float dir = sprite.params.z < 0.0 ? -1.0 : 1.0;
        vec2 q = vec2(p.x * dir, p.y);
        vec2 v[kMaxPolygonVertices];
        v[0] = vec2(-s * 0.55, -s * 0.25);
        v[1] = vec2(0.0, -s * 0.25);
        v[2] = vec2(0.0, -s * 0.6);
        v[3] = vec2(s * 0.62, 0.0);
        v[4] = vec2(0.0, s * 0.6);
        v[5] = vec2(0.0, s * 0.25);
        v[6] = vec2(-s * 0.55, s * 0.25);
        part(sdPolygon(q, v, 7), base);
    } else {
        part(sdCircle(p, s * 0.42), base);
        fill(sdCircle(p, s * 0.2), accent);
    }
    outColor = acc;
}
