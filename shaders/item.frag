#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// Power-ups and pickups by kind (params.x):
//   0 mushroom (grow)   1 flower (fire)   2 star   3 1-up mushroom
//   4 star coin (params.y: 1 = already found on an earlier visit: a ghost)
//   5 fireball   6 the King's fireball   7 a coin popping out of a block
// color / color2: base and accent. The item fills the quad minus params.w.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.05, 0.03, 0.05);
const float kPi = 3.14159265;

vec4 acc = vec4(0.0);

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 0.8)), acc);
    acc = over(vec4(rgb, coverage(d)), acc);
}

void fill(float d, vec3 rgb)
{
    acc = over(vec4(rgb, coverage(d)), acc);
}

float sdStar(vec2 p, float r, float inner)
{
    vec2 v[kMaxPolygonVertices];
    for (int i = 0; i < 10; ++i) {
        float a = -kPi * 0.5 + float(i) * kPi / 5.0;
        float rr = (i % 2 == 0) ? r : r * inner;
        v[i] = vec2(cos(a), sin(a)) * rr;
    }
    return sdPolygon(p, v, 10);
}

void main()
{
    vec2 p = spriteLocal(inUv);
    int kind = int(sprite.params.x + 0.5);
    vec3 base = sprite.color.rgb;
    vec3 accent = sprite.color2.rgb;
    if (kind == 0 || kind == 3) {
        // A mushroom: spotted cap, pale stem with eyes.
        part(sdRoundBox(p - vec2(0.0, 3.2), vec2(4.2, 3.4), 2.0), vec3(1.0, 0.93, 0.8));
        fill(sdEllipse(p - vec2(-1.5, 3.0), vec2(0.6, 1.2)), kInk);
        fill(sdEllipse(p - vec2(1.5, 3.0), vec2(0.6, 1.2)), kInk);
        float cap = max(sdEllipse(p - vec2(0.0, 0.2), vec2(7.0, 6.2)), p.y - 0.8);
        part(cap, base);
        fill(max(sdCircle(p - vec2(0.0, -3.0), 2.2), cap), accent);
        fill(max(sdCircle(p - vec2(-5.0, -0.8), 1.6), cap), accent);
        fill(max(sdCircle(p - vec2(5.0, -0.8), 1.6), cap), accent);
    } else if (kind == 1) {
        // A fire flower: stem, leaves, four petals and a face.
        part(sdSegment(p, vec2(0.0, 7.0), vec2(0.0, 1.0), 1.0), vec3(0.2, 0.65, 0.2));
        part(sdEllipse(p - vec2(-2.8, 5.2), vec2(2.6, 1.1)), vec3(0.3, 0.78, 0.3));
        part(sdEllipse(p - vec2(2.8, 5.2), vec2(2.6, 1.1)), vec3(0.3, 0.78, 0.3));
        vec2 h = p - vec2(0.0, -2.6);
        float pulse = 0.5 + 0.5 * sin(sprite.time * 10.0);
        part(sdEllipse(h, vec2(6.6, 4.4)), mix(base, accent, pulse));
        part(sdEllipse(h, vec2(4.6, 3.0)), accent);
        part(sdEllipse(h, vec2(2.8, 1.8)), vec3(1.0, 0.98, 0.85));
        fill(sdEllipse(h - vec2(-1.0, 0.0), vec2(0.4, 0.9)), kInk);
        fill(sdEllipse(h - vec2(1.0, 0.0), vec2(0.4, 0.9)), kInk);
    } else if (kind == 2) {
        float glowT = 0.5 + 0.5 * sin(sprite.time * 14.0);
        part(sdStar(p, 7.4, 0.48) - 0.6, mix(base, accent, glowT));
        fill(sdEllipse(p - vec2(-1.4, -0.5), vec2(0.6, 1.3)), kInk);
        fill(sdEllipse(p - vec2(1.4, -0.5), vec2(0.6, 1.3)), kInk);
    } else if (kind == 4) {
        // A star coin: a big coin with a star, turning slowly.
        float turn = max(abs(cos(sprite.time * 1.8)), 0.25);
        vec2 q = vec2(p.x / turn, p.y);
        float ghost = sprite.params.y;
        float d = sdCircle(q, 9.0) * turn;
        vec3 gold = mix(base, vec3(0.55, 0.65, 0.9), ghost);
        acc = over(vec4(kInk, coverage(d - 0.9) * (1.0 - ghost * 0.6)), acc);
        acc = over(vec4(gold * 0.8, coverage(d) * (1.0 - ghost * 0.5)), acc);
        acc = over(vec4(gold, coverage(sdCircle(q, 7.4) * turn) * (1.0 - ghost * 0.5)), acc);
        acc = over(vec4(mix(accent, vec3(1.0), ghost), coverage(sdStar(q, 5.6, 0.45) * turn) * (1.0 - ghost * 0.4)), acc);
    } else if (kind == 5 || kind == 6) {
        // A fireball: a spinning swirl of flame.
        float a = sprite.time * 18.0;
        vec2 r = vec2(p.x * cos(a) - p.y * sin(a), p.x * sin(a) + p.y * cos(a));
        float size = kind == 6 ? 4.6 : 3.4;
        float flame = sdCircle(r, size) - 0.8 * sin(atan(r.y, r.x) * 3.0);
        acc = over(vec4(base * 1.5, coverage(flame)), acc);
        acc = over(vec4(accent * 1.6, coverage(sdCircle(r, size * 0.5))), acc);
    } else {
        // A coin popped out of a block.
        float spin = sprite.params.y > 0.5 ? 1.0 : max(abs(cos(sprite.time * 16.0)), 0.15);
        vec2 q = vec2(p.x / spin, p.y);
        float d = sdEllipse(q, vec2(4.6, 6.2)) * spin;
        part(d, base);
        fill(sdEllipse(q, vec2(3.0, 4.6)) * spin, accent);
    }
    outColor = acc;
}
