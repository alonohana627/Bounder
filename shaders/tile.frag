#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// Every tile of a level, by kind (params.x):
//   0 ground (theme in params.y: 0 grass, 1 sand, 2 snow, 3 cloud, 4 fortress
//     stone, 5 castle stone, 6 bonus-room bricks): neighbour-aware edges,
//     a grass / sand / snow cap on open tops, speckled fill
//   1 hard block   2 brick   3 "?" block   4 used block
//   5 pipe top (params.z: 0 left half, 1 right half)   6 pipe body (same)
//   7 one-way platform
//   8-13 slopes: / \ (45 degrees), a A (22.5 rising right), D d (22.5 falling right)
//   14 coin (spins)   15 spikes   16 lava   17 rail across   18 rail up and down
// color = main fill, color2 = cap / accent. bits = neighbour mask | variant
// << 8 | cell x << 32 | cell y << 48 (draw::drawTiles). A tile is 16 units.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

// A 0..1 hash of a point's whole-unit cell (GameLib's hash12 takes integers).
float hashf(vec2 p)
{
    return hash12(uvec2(ivec2(floor(p))));
}

const uint N = 1u, E = 2u, S = 4u, W = 8u;
const vec3 kInk = vec3(0.045, 0.03, 0.05);

uint maskBits() { return sprite.bits.x & 0xffu; }
bool isOpen(uint bit) { return (maskBits() & bit) == 0u; }
float variant() { return float((sprite.bits.x >> 8) & 0xffu) / 255.0; }
vec2 cellOrigin() { return vec2(float(sprite.bits.y & 0xffffu), float(sprite.bits.y >> 16)) * 16.0; }

vec4 put(vec4 acc, vec3 rgb, float cover) { return over(vec4(rgb, cover), acc); }

// The ground's fill: speckles, darker towards the bottom, a pattern by theme.
vec3 groundFill(vec2 wp, int theme)
{
    vec3 base = sprite.color.rgb;
    if (theme == 4 || theme == 5 || theme == 6) {
        // Stone or brick courses with mortar.
        float rowH = theme == 6 ? 8.0 : 8.0;
        float row = floor(wp.y / rowH);
        float bx = wp.x + (mod(row, 2.0) * 0.5) * 16.0;
        vec2 inBrick = vec2(mod(bx, 16.0), mod(wp.y, rowH));
        float mortar = step(inBrick.x, 1.0) + step(inBrick.y, 1.0);
        float shade = 0.9 + 0.12 * hashf(vec2(floor(bx / 16.0), row));
        vec3 brick = base * shade;
        brick += vec3(0.05) * step(rowH - 2.0, inBrick.y) * -1.0;
        return mix(brick, base * 0.45, clamp(mortar, 0.0, 1.0));
    }
    if (theme == 3) {
        // Cloud: soft white puffs.
        float puff = valueNoise(wp * 0.18) * 0.5 + valueNoise(wp * 0.07) * 0.5;
        return mix(base * 0.92, base, smoothstep(0.35, 0.7, puff));
    }
    float n = valueNoise(wp * 0.35);
    float speck = step(0.82, hashf(floor(wp / 3.0)));
    vec3 c = base * (0.92 + 0.12 * n);
    c = mix(c, base * 0.8, speck * 0.35);
    // Pebbles in dirt and sand.
    vec2 cellP = mod(wp, 8.0) - 4.0;
    float pebble = 1.0 - smoothstep(0.9, 1.4, length(cellP - vec2(hashf(floor(wp / 8.0)) * 3.0 - 1.5, 0.0)));
    c = mix(c, base * 1.18, pebble * 0.35 * step(0.6, hashf(floor(wp / 8.0) + 7.0)));
    return c;
}

// The cap on an open top: grass blades, sand ripples, snow drifts, cloud fluff.
// Returns the cap's coverage at distance `t` below the surface along `wp`.
float capCover(float t, vec2 wp, int theme)
{
    float depth = 4.0;
    float wobble = 0.0;
    if (theme == 0) {
        depth = 4.2;
        wobble = 0.9 * sin(wp.x * 1.3) + 0.6 * sin(wp.x * 2.9 + 1.0);
    } else if (theme == 1) {
        depth = 3.4;
        wobble = 0.6 * sin(wp.x * 0.6);
    } else if (theme == 2) {
        depth = 5.0;
        wobble = 1.2 * sin(wp.x * 0.45) + 0.5 * sin(wp.x * 1.7);
    } else if (theme == 3) {
        depth = 3.5;
        wobble = 0.8 * sin(wp.x * 0.9);
    } else {
        depth = 2.6;
    }
    return coverage(t - (depth + wobble));
}

vec4 ground(vec2 p, int theme)
{
    vec2 wp = cellOrigin() + p + 8.0;
    // The tile's own box, pushed far past every connected side so joined
    // tiles show no seams; only open sides get an edge and round corners.
    float l = isOpen(W) ? -8.0 : -40.0;
    float r = isOpen(E) ? 8.0 : 40.0;
    float t = isOpen(N) ? -8.0 : -40.0;
    float b = isOpen(S) ? 8.0 : 40.0;
    vec2 c = vec2(l + r, t + b) * 0.5;
    vec2 h = vec2(r - l, b - t) * 0.5;
    float d = sdRoundBox(p - c, h, theme == 3 ? 4.5 : 2.5);
    vec4 acc = vec4(0.0);
    acc = put(acc, kInk, coverage(d));
    vec3 fill = groundFill(wp, theme);
    acc = put(acc, fill, coverage(d + 1.1));
    if (isOpen(N)) {
        float below = p.y + 8.0; // distance under the top
        vec3 cap = sprite.color2.rgb * (0.95 + 0.1 * valueNoise(wp * 0.6));
        float inside = coverage(d + 1.1);
        // A darker rim under the cap, the cap, a light line along the top.
        acc = put(acc, mix(cap, kInk, 0.5), capCover(below - 1.3, wp, theme) * inside);
        acc = put(acc, cap, capCover(below, wp, theme) * inside);
        acc = put(acc, min(cap * 1.3, vec3(1.0)), (1.0 - smoothstep(0.0, 1.4, below - 1.2)) * inside * 0.5);
        if (theme == 0) {
            // Darker tufts in the grass.
            float tuft = step(0.7, hashf(vec2(floor(wp.x / 2.0), 5.0))) * step(below, 3.0) * step(1.6, below);
            acc = put(acc, cap * 0.75, tuft * inside);
        }
    }
    return acc;
}

float sdQuestion(vec2 p)
{
    // A "?" from an arc, a stem and a dot (p in block units, about 10 tall).
    vec2 q = p - vec2(0.0, -2.2);
    float arc = abs(length(q) - 2.6) - 0.95;
    arc = max(arc, -max(q.x, -q.y - 0.5)); // the hook is open at its lower left
    float stem = sdSegment(p, vec2(0.0, 0.4), vec2(0.0, 1.9), 0.95);
    float dot = sdCircle(p - vec2(0.0, 4.2), 1.05);
    return min(min(arc, stem), dot);
}

vec4 block(vec2 p, int kind)
{
    vec4 acc = vec4(0.0);
    float d = sdRoundBox(p, vec2(8.0), 1.8);
    acc = put(acc, kInk, coverage(d));
    vec3 base = sprite.color.rgb;
    if (kind == 1) {
        // Hard block: a bevelled stone.
        acc = put(acc, base, coverage(d + 1.0));
        float bevel = sdRoundBox(p, vec2(5.0), 1.0);
        vec3 light = min(base * 1.35, vec3(1.0));
        vec3 dark = base * 0.6;
        float diag = p.x + p.y; // < 0: upper left
        acc = put(acc, diag < 0.0 ? light : dark, coverage(d + 1.0) * (1.0 - coverage(bevel)));
        acc = put(acc, base, coverage(bevel));
        return acc;
    }
    if (kind == 2) {
        // Brick: courses of bricks with mortar.
        acc = put(acc, base * 0.55, coverage(d + 1.0));
        vec2 q = p + 8.0;
        float row = floor(q.y / 5.33);
        float off = mod(row, 2.0) * 4.0;
        vec2 inB = vec2(mod(q.x + off, 8.0), mod(q.y, 5.33));
        float brick = sdRoundBox(inB - vec2(4.0, 2.66), vec2(3.3, 2.0), 0.6);
        float shade = 0.92 + 0.14 * hashf(vec2(floor((q.x + off) / 8.0), row));
        acc = put(acc, base * shade, coverage(brick) * coverage(d + 1.0));
        acc = put(acc, min(base * 1.3, vec3(1.0)), coverage(brick) * coverage(d + 1.0) * step(inB.y, 1.4) * 0.6);
        return acc;
    }
    if (kind == 3) {
        // "?" block: gold, rivets, a shining question mark.
        float shine = 0.5 + 0.5 * sin(sprite.time * 3.0 + cellOrigin().x * 0.05);
        vec3 gold = base * (0.95 + 0.1 * shine);
        acc = put(acc, gold, coverage(d + 1.0));
        float inner = sdRoundBox(p, vec2(6.4), 1.4);
        acc = put(acc, gold * 0.78, coverage(d + 1.0) * (1.0 - coverage(inner)) * step(0.0, p.x + p.y));
        acc = put(acc, min(gold * 1.25, vec3(1.0)), coverage(d + 1.0) * (1.0 - coverage(inner)) * step(p.x + p.y, 0.0));
        for (int i = 0; i < 4; ++i) {
            vec2 rp = vec2(i % 2 == 0 ? -5.6 : 5.6, i < 2 ? -5.6 : 5.6);
            acc = put(acc, gold * 0.55, coverage(sdCircle(p - rp, 0.8)));
        }
        float q = sdQuestion(p);
        acc = put(acc, kInk, coverage(q - 0.3) * 0.55);
        acc = put(acc, mix(vec3(1.0, 0.98, 0.9), sprite.color2.rgb, 0.3), coverage(q + 0.25));
        return acc;
    }
    // Used block: dull, riveted.
    acc = put(acc, base, coverage(d + 1.0));
    float inner = sdRoundBox(p, vec2(6.0), 1.4);
    acc = put(acc, base * 0.8, coverage(d + 1.0) * (1.0 - coverage(inner)));
    for (int i = 0; i < 4; ++i) {
        vec2 rp = vec2(i % 2 == 0 ? -5.4 : 5.4, i < 2 ? -5.4 : 5.4);
        acc = put(acc, base * 0.5, coverage(sdCircle(p - rp, 0.9)));
    }
    return acc;
}

vec4 pipe(vec2 p, int kind)
{
    bool right = sprite.params.z > 0.5;
    // Across the whole 32-unit pipe: u = 0 at its left edge, 1 at its right.
    float x = p.x + (right ? 8.0 : -8.0);  // -16..16 across the pipe
    bool top = kind == 5;
    float halfW = top ? 15.5 : 13.5;
    float d;
    if (top) {
        d = sdRoundBox(vec2(x, p.y), vec2(halfW, 7.5), 1.6);
    } else {
        d = abs(x) - halfW; // the body runs on into its neighbours
    }
    vec4 acc = vec4(0.0);
    acc = put(acc, kInk, coverage(d));
    float u = clamp((x + halfW) / (2.0 * halfW), 0.0, 1.0);
    // A cylinder: a bright band left of center, dark at the right.
    float light = 0.65 + 0.55 * exp(-pow((u - 0.3) / 0.12, 2.0)) - 0.35 * smoothstep(0.65, 1.0, u)
                  - 0.2 * smoothstep(0.15, 0.0, u);
    vec3 rgb = sprite.color.rgb * light;
    acc = put(acc, rgb, coverage(d + 1.1));
    if (top) {
        // The rim's lower edge.
        acc = put(acc, kInk * 0.8 + rgb * 0.2, coverage(abs(p.y - 6.5) - 0.6) * coverage(d + 1.1));
    }
    return acc;
}

vec4 platform(vec2 p, int theme)
{
    float l = isOpen(W) ? -8.0 : -40.0;
    float r = isOpen(E) ? 8.0 : 40.0;
    if (theme == 3) {
        // In the sky: a puffy cloud, flat on top so it reads as a floor.
        vec2 wp = cellOrigin() + p;
        float bumps = 1.6 * sin(wp.x * 0.55) + 1.0 * sin(wp.x * 1.3 + 0.7);
        float d = sdRoundBox(p - vec2((l + r) * 0.5, -3.5), vec2((r - l) * 0.5, 4.0 + max(bumps, 0.0) * 0.4), 4.0);
        vec4 acc = vec4(0.0);
        acc = put(acc, mix(kInk, sprite.color2.rgb, 0.55), coverage(d));
        acc = put(acc, sprite.color2.rgb * (0.95 + 0.05 * valueNoise(wp * 0.3)), coverage(d + 1.0));
        acc = put(acc, sprite.color2.rgb * 0.88, coverage(d + 1.0) * smoothstep(-1.0, 3.0, p.y));
        return acc;
    }
    // A plank across the top half; open ends are rounded.
    vec2 c = vec2((l + r) * 0.5, -4.5);
    float d = sdRoundBox(p - c, vec2((r - l) * 0.5, 3.5), 2.5);
    vec4 acc = vec4(0.0);
    acc = put(acc, kInk, coverage(d));
    vec2 wp = cellOrigin() + p;
    vec3 wood = sprite.color.rgb * (0.9 + 0.1 * sin(wp.x * 0.9 + sin(wp.y * 2.0)));
    acc = put(acc, wood, coverage(d + 1.0));
    acc = put(acc, sprite.color2.rgb, coverage(d + 1.0) * step(p.y, -6.2));
    // Supports under the open ends.
    if (isOpen(W)) {
        acc = put(acc, sprite.color.rgb * 0.6, coverage(sdBox(p - vec2(-4.5, 3.0), vec2(1.0, 5.5))) * 0.8);
    }
    if (isOpen(E)) {
        acc = put(acc, sprite.color.rgb * 0.6, coverage(sdBox(p - vec2(4.5, 3.0), vec2(1.0, 5.5))) * 0.8);
    }
    return acc;
}

vec4 slope(vec2 p, int kind, int theme)
{
    // The surface height across the tile (y down; -8 top, 8 bottom).
    float yl = 8.0, yr = -8.0;
    if (kind == 9) { yl = -8.0; yr = 8.0; }
    if (kind == 10) { yl = 8.0; yr = 0.0; }
    if (kind == 11) { yl = 0.0; yr = -8.0; }
    if (kind == 12) { yl = -8.0; yr = 0.0; }
    if (kind == 13) { yl = 0.0; yr = 8.0; }
    float k = (yr - yl) / 16.0;
    float surface = yl + (p.x + 8.0) * k;
    float d = (surface - p.y) / sqrt(1.0 + k * k); // < 0 below the surface
    vec2 wp = cellOrigin() + p + 8.0;
    vec4 acc = vec4(0.0);
    acc = put(acc, kInk, coverage(d));
    acc = put(acc, groundFill(wp, theme), coverage(d + 1.1));
    float below = -d - 1.0;
    acc = put(acc, sprite.color2.rgb, capCover(below, wp, theme) * coverage(d + 1.1));
    acc = put(acc, min(sprite.color2.rgb * 1.3, vec3(1.0)), (1.0 - smoothstep(0.0, 1.2, below - 0.2)) * coverage(d + 1.1) * 0.5);
    return acc;
}

vec4 coin(vec2 p)
{
    float phase = sprite.time * 4.0 + cellOrigin().x * 0.07;
    float squeeze = max(abs(cos(phase)), 0.12);
    vec2 q = vec2(p.x / squeeze, p.y);
    float d = sdEllipse(q, vec2(5.0, 6.6)) * squeeze;
    vec4 acc = vec4(0.0);
    acc = put(acc, kInk, coverage(d));
    vec3 gold = sprite.color.rgb;
    acc = put(acc, gold * 0.75, coverage(d + 0.9));
    acc = put(acc, gold, coverage(sdEllipse(q, vec2(3.6, 5.2)) * squeeze));
    acc = put(acc, vec3(1.0, 0.97, 0.8), coverage(sdSegment(q, vec2(-1.2, -2.6), vec2(-1.2, 1.0), 0.7) * squeeze) * step(0.0, cos(phase)));
    return acc;
}

vec4 spikes(vec2 p)
{
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 2; ++i) {
        float cx = i == 0 ? -4.0 : 4.0;
        vec2 v[kMaxPolygonVertices];
        v[0] = vec2(cx - 3.8, 8.0);
        v[1] = vec2(cx, -6.5);
        v[2] = vec2(cx + 3.8, 8.0);
        float d = sdPolygon(p, v, 3);
        acc = put(acc, kInk, coverage(d));
        vec3 metal = sprite.color.rgb * (p.x < cx ? 1.15 : 0.8);
        acc = put(acc, metal, coverage(d + 1.0));
    }
    return acc;
}

vec4 lava(vec2 p)
{
    vec2 wp = cellOrigin() + p + 8.0;
    float top = isOpen(N) ? -5.0 + 1.2 * sin(wp.x * 0.4 + sprite.time * 2.5) : -40.0;
    float d = top - p.y;
    float glowN = valueNoise(wp * 0.12 + vec2(sprite.time * 0.6, 0.0));
    vec3 hot = mix(sprite.color.rgb, sprite.color2.rgb, smoothstep(0.3, 0.8, glowN));
    vec4 acc = vec4(0.0);
    acc = put(acc, hot * 1.4, coverage(d));
    acc = put(acc, sprite.color2.rgb * 1.6, coverage(abs(d + 0.8) - 0.8) * step(-30.0, top));
    float bubble = sdCircle(vec2(mod(wp.x + 3.0, 16.0) - 8.0, p.y - top - 4.0 + fract(sprite.time * 0.5 + variant()) * -3.0), 1.4);
    acc = put(acc, sprite.color2.rgb * 1.5, coverage(bubble) * coverage(d) * step(0.5, variant()));
    return acc;
}

vec4 rail(vec2 p, bool across)
{
    vec2 q = across ? p : p.yx;
    float dots = abs(mod(q.x + 2.0, 4.0) - 2.0);
    float d = max(sdCircle(vec2(dots, q.y), 0.9), -1.0);
    return vec4(sprite.color.rgb, coverage(d) * 0.9);
}

void main()
{
    vec2 p = spriteLocal(inUv);
    int kind = int(sprite.params.x + 0.5);
    int theme = int(sprite.params.y + 0.5);
    vec4 c;
    if (kind == 0) {
        c = ground(p, theme);
    } else if (kind >= 1 && kind <= 4) {
        c = block(p, kind);
    } else if (kind == 5 || kind == 6) {
        c = pipe(p, kind);
    } else if (kind == 7) {
        c = platform(p, theme);
    } else if (kind >= 8 && kind <= 13) {
        c = slope(p, kind, theme);
    } else if (kind == 14) {
        c = coin(p);
    } else if (kind == 15) {
        c = spikes(p);
    } else if (kind == 16) {
        c = lava(p);
    } else {
        c = rail(p, kind == 17);
    }
    outColor = vec4(dither(c.rgb, gl_FragCoord.xy, 0.01), c.a * sprite.color.a);
}
