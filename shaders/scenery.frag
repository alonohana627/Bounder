#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// Level furniture by kind (params.x):
//   0 the goal pole: a pole with a ball on top and a flag that slides down
//     (params.y: flag position 0 top .. 1 bottom); the quad is the pole's
//     full height, 2 tiles wide
//   1 the checkpoint flag (params.y: 1 once reached; it then waves the
//     hero's colors)
//   2 the castle at the end of a level (color = walls, color2 = roofs)
//   3 a moving lift (3 tiles x half a tile)   4 a falling platform
//     (params.y: 0..1 how close it is to dropping: it shakes and reddens)
// color = main, color2 = accent.

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.05, 0.03, 0.05);

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

void main()
{
    vec2 p = spriteLocal(inUv);
    vec2 size = sprite.size;
    int kind = int(sprite.params.x + 0.5);
    vec3 base = sprite.color.rgb;
    vec3 accent = sprite.color2.rgb;
    if (kind == 0) {
        float top = -size.y * 0.5;
        float bottom = size.y * 0.5;
        // The flag hangs left of the pole and slides down.
        float flagY = mix(top + 10.0, bottom - 12.0, clamp(sprite.params.y, 0.0, 1.0));
        float wave = sin(sprite.time * 6.0 + p.x * 0.4) * 0.8;
        vec2 v[kMaxPolygonVertices];
        v[0] = vec2(-0.5, flagY - 5.0);
        v[1] = vec2(-13.0, flagY + wave);
        v[2] = vec2(-0.5, flagY + 5.0);
        part(sdPolygon(p, v, 3), accent);
        fill(sdCircle(p - vec2(-5.0, flagY), 1.6), vec3(1.0));
        part(sdBox(p - vec2(0.0, 2.0), vec2(1.0, size.y * 0.5 - 4.0)), base);
        part(sdCircle(p - vec2(0.0, top + 3.5), 2.8), vec3(1.0, 0.85, 0.25));
        part(sdRoundBox(p - vec2(0.0, bottom - 3.0), vec2(4.0, 3.0), 0.8), base * 0.7);
    } else if (kind == 1) {
        bool reached = sprite.params.y > 0.5;
        float wave = sin(sprite.time * 5.0 + p.x * 0.5) * (reached ? 1.0 : 0.3);
        float top = -size.y * 0.5 + 3.0;
        vec2 v[kMaxPolygonVertices];
        v[0] = vec2(0.5, top);
        v[1] = vec2(12.0, top + 4.0 + wave);
        v[2] = vec2(0.5, top + 8.0);
        part(sdPolygon(p, v, 3), reached ? accent : base * 0.85);
        part(sdBox(p - vec2(0.0, 1.0), vec2(0.9, size.y * 0.5 - 2.0)), vec3(0.9, 0.9, 0.95));
        part(sdCircle(p - vec2(0.0, top - 1.0), 1.8), reached ? vec3(1.0, 0.85, 0.25) : vec3(0.75));
    } else if (kind == 2) {
        // A small castle: two towers, a keep, a door, flags.
        float ground = size.y * 0.5;
        float keep = sdBox(p - vec2(0.0, ground - 22.0), vec2(26.0, 22.0));
        float crenel = abs(mod(p.x + 2.5, 10.0) - 5.0) - 2.5;
        keep = min(keep, max(sdBox(p - vec2(0.0, ground - 47.0), vec2(26.0, 3.0)), crenel));
        part(keep, base);
        for (int s = -1; s <= 1; s += 2) {
            vec2 t = p - vec2(float(s) * 24.0, ground - 36.0);
            part(sdBox(t, vec2(8.0, 36.0)), base * 0.92);
            vec2 v[kMaxPolygonVertices];
            v[0] = vec2(-10.0, -34.0);
            v[1] = vec2(0.0, -52.0);
            v[2] = vec2(10.0, -34.0);
            part(sdPolygon(t, v, 3), accent);
            fill(sdRoundBox(t - vec2(0.0, -14.0), vec2(2.0, 4.0), 2.0), kInk);
        }
        // Stone courses.
        float row = floor((p.y - ground) / 6.0);
        float mortar = step(abs(mod(p.y - ground, 6.0)), 0.6)
                       + step(abs(mod(p.x + mod(row, 2.0) * 5.0, 10.0)), 0.6);
        acc = over(vec4(base * 0.6, clamp(mortar, 0.0, 1.0) * coverage(keep) * 0.35), acc);
        // The door.
        part(min(sdBox(p - vec2(0.0, ground - 7.0), vec2(6.0, 7.0)), sdCircle(p - vec2(0.0, ground - 14.0), 6.0)), kInk * 1.5);
    } else if (kind == 3 || kind == 4) {
        float shakeAmount = kind == 4 ? sprite.params.y : 0.0;
        vec3 c = kind == 4 ? mix(base, vec3(0.95, 0.35, 0.25), shakeAmount * 0.6) : base;
        float d = sdRoundBox(p, size * 0.5 - vec2(0.5), 3.0);
        part(d, c);
        fill(sdRoundBox(p - vec2(0.0, -size.y * 0.2), size * vec2(0.5, 0.18) - vec2(2.0, 0.0), 1.5), min(c * 1.3, vec3(1.0)));
        // Bolts.
        for (int i = -1; i <= 1; ++i) {
            fill(sdCircle(p - vec2(float(i) * size.x * 0.33, 0.8), 0.9), accent);
        }
    }
    outColor = acc;
}
