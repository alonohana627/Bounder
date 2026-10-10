#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// Every enemy, rounded and outlined, by kind (params.x):
//   0 walker (a grumpy chestnut)   1 shell walker   2 shell (alone)
//   3 hopper (a frog)   4 flier (a winged puff)   5 plant (head on a stem)
//   6 spiky   7 the Brute (fortress boss)   8 the King (final boss)
// color = body, color2 = accent (shell, belly, crown). params: y facing,
// z animation phase 0..1 (walk, flap, chomp, spin), w margin. bits.x: bit 0
// squashed flat, bit 1 hurt (flashes), bit 2 angry (boss's last phase).
// The body's feet stand on the quad's bottom minus the margin.

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

void eye(vec2 q, vec2 at, float r, float look, bool angry)
{
    fill(sdEllipse(q - at, vec2(r * 0.75, r)), vec3(1.0));
    fill(sdCircle(q - at - vec2(look * r * 0.3, r * 0.2), r * 0.45), kInk);
    if (angry) {
        fill(sdSegment(q, at + vec2(-r, -r * 1.3), at + vec2(r, -r * 0.7), r * 0.28), kInk);
    }
}

void shell(vec2 c, vec2 radii, vec3 body, vec3 rim, float spin)
{
    part(sdEllipse(c, radii), body);
    // Hexagon plates that slide round while it spins.
    float off = fract(spin) * radii.x * 1.2;
    for (int i = -1; i <= 1; ++i) {
        vec2 plate = c - vec2(float(i) * radii.x * 0.6 + off - radii.x * 0.0, -radii.y * 0.15);
        float d = max(sdCircle(plate, radii.y * 0.42), sdEllipse(c, radii * 0.88));
        fill(d, body * 1.25);
    }
    fill(max(sdEllipse(c - vec2(0.0, radii.y * 0.55), vec2(radii.x, radii.y * 0.45)), sdEllipse(c, radii)), rim);
}

void main()
{
    vec2 p = spriteLocal(inUv);
    int kind = int(sprite.params.x + 0.5);
    float facing = sprite.params.y < 0.0 ? -1.0 : 1.0;
    float phase = sprite.params.z;
    float margin = sprite.params.w;
    float height = sprite.size.y - 2.0 * margin;
    bool squashed = (sprite.bits.x & 1u) != 0u;
    bool hurt = (sprite.bits.x & 2u) != 0u;
    bool angry = (sprite.bits.x & 4u) != 0u;
    vec2 q = vec2(p.x * facing, p.y - height * 0.5); // feet at y = 0, facing +x
    vec3 body = sprite.color.rgb;
    vec3 accent = sprite.color2.rgb;
    if (hurt && fract(sprite.time * 12.0) < 0.5) {
        body = mix(body, vec3(1.0), 0.7);
        accent = mix(accent, vec3(1.0), 0.7);
    }
    float walk = sin(phase * 2.0 * kPi);

    if (kind == 0) {
        if (squashed) {
            part(sdEllipse(q - vec2(0.0, -2.5), vec2(7.5, 2.6)), body);
            fill(sdSegment(q, vec2(-3.0, -2.8), vec2(-1.0, -2.2), 0.45), kInk);
            fill(sdSegment(q, vec2(1.0, -2.2), vec2(3.0, -2.8), 0.45), kInk);
        } else {
            part(sdEllipse(q - vec2(-3.0 - walk * 1.5, -1.3), vec2(2.8, 1.6)), accent * 0.6);
            part(sdEllipse(q - vec2(3.0 + walk * 1.5, -1.3), vec2(2.8, 1.6)), accent * 0.6);
            // A chestnut: wide at the bottom, a cap on top.
            part(sdRoundBox(q - vec2(0.0, -4.6), vec2(4.2, 2.6), 2.2), accent);
            part(max(sdEllipse(q - vec2(0.0, -8.0), vec2(7.2, 5.6)), q.y + 5.0), body);
            eye(q, vec2(-1.8, -7.0), 1.5, 0.6, true);
            eye(q, vec2(2.0, -7.0), 1.5, 0.6, true);
            fill(sdSegment(q, vec2(-1.2, -4.4), vec2(1.6, -4.4), 0.45), kInk);
        }
    } else if (kind == 1) {
        // A turtle walking upright: feet, shell on the back, head forward.
        part(sdEllipse(q - vec2(-2.5 - walk * 1.2, -1.4), vec2(2.4, 1.5)), vec3(1.0, 0.85, 0.35));
        part(sdEllipse(q - vec2(2.5 + walk * 1.2, -1.4), vec2(2.4, 1.5)), vec3(1.0, 0.85, 0.35));
        part(sdRoundBox(q - vec2(1.0, -9.0), vec2(2.6, 6.0), 2.4), vec3(1.0, 0.86, 0.4));
        shell(q - vec2(-1.6, -7.8), vec2(5.4, 5.0), body, accent, 0.0);
        part(sdCircle(q - vec2(3.6, -15.2), 3.4), vec3(1.0, 0.86, 0.4));
        eye(q, vec2(4.6, -16.0), 1.3, 0.8, false);
    } else if (kind == 2) {
        shell(q - vec2(0.0, -5.6), vec2(7.0, 5.4), body, accent, phase);
    } else if (kind == 3) {
        // A frog: crouched when resting (phase 0), stretched in the air.
        float stretch = phase;
        part(sdEllipse(q - vec2(-3.6, -1.6), vec2(3.0 - stretch, 1.6)), body * 0.8);
        part(sdEllipse(q - vec2(3.6, -1.6), vec2(3.0 - stretch, 1.6)), body * 0.8);
        part(sdEllipse(q - vec2(0.0, -6.0 - stretch * 2.0), vec2(7.0 - stretch * 1.4, 4.6 + stretch * 1.8)), body);
        fill(sdEllipse(q - vec2(0.8, -4.8 - stretch * 2.0), vec2(4.2, 2.6)), accent);
        eye(q, vec2(-2.5, -10.5 - stretch * 3.0), 1.9, 0.5, false);
        eye(q, vec2(2.8, -10.5 - stretch * 3.0), 1.9, 0.5, false);
    } else if (kind == 4) {
        // A winged puff.
        float flap = sin(phase * 2.0 * kPi) * 0.6;
        vec2 c = q - vec2(0.0, -6.5);
        for (int s = -1; s <= 1; s += 2) {
            vec2 w = c - vec2(float(s) * 6.0, -2.0);
            float a = flap * float(s);
            w = vec2(w.x * cos(a) - w.y * sin(a), w.x * sin(a) + w.y * cos(a));
            part(sdEllipse(w, vec2(4.0, 2.2)), vec3(1.0));
        }
        part(sdCircle(c, 6.0), body);
        fill(sdEllipse(c - vec2(0.8, 2.6), vec2(3.4, 2.0)), accent);
        eye(c, vec2(-1.8, -1.0), 1.4, 0.7, true);
        eye(c, vec2(2.0, -1.0), 1.4, 0.7, true);
    } else if (kind == 5) {
        // A plant: stem and leaves, a spotted head that chomps.
        float chomp = 0.5 + 0.5 * sin(phase * 2.0 * kPi);
        part(sdSegment(q, vec2(0.0, 0.0), vec2(0.0, -11.0), 1.4), vec3(0.2, 0.62, 0.22));
        part(sdEllipse(q - vec2(-3.0, -5.0), vec2(3.0, 1.4)), vec3(0.3, 0.75, 0.28));
        part(sdEllipse(q - vec2(3.0, -6.0), vec2(3.0, 1.4)), vec3(0.3, 0.75, 0.28));
        vec2 h = q - vec2(0.0, -16.0);
        float head = sdCircle(h, 6.2);
        float mouth = sdEllipse(h - vec2(0.0, 1.0), vec2(5.5, 1.0 + chomp * 3.0));
        part(max(head, -mouth), body);
        fill(max(sdCircle(h - vec2(-2.4, -3.0), 1.3), head), vec3(1.0));
        fill(max(sdCircle(h - vec2(2.8, -2.0), 1.1), head), vec3(1.0));
        // Teeth along the lips.
        float teeth = max(abs(mod(h.x + 1.0, 2.0) - 1.0) * 1.5 + abs(abs(h.y - 1.0) - (1.0 + chomp * 3.0)) - 1.4, mouth - 0.4);
        fill(max(teeth, head), vec3(1.0));
    } else if (kind == 6) {
        // Spiky: a shell covered in spikes, little feet.
        part(sdEllipse(q - vec2(-2.4 - walk, -1.3), vec2(2.2, 1.4)), vec3(1.0, 0.8, 0.4));
        part(sdEllipse(q - vec2(2.4 + walk, -1.3), vec2(2.2, 1.4)), vec3(1.0, 0.8, 0.4));
        vec2 c = q - vec2(0.0, -6.0);
        for (int i = 0; i < 5; ++i) {
            float a = kPi * (1.1 + 0.2 * float(i));
            vec2 dir = vec2(cos(a), sin(a));
            vec2 v[kMaxPolygonVertices];
            v[0] = dir * 9.6;
            v[1] = dir * 4.5 + vec2(-dir.y, dir.x) * 2.0;
            v[2] = dir * 4.5 - vec2(-dir.y, dir.x) * 2.0;
            part(sdPolygon(c, v, 3), vec3(0.96, 0.94, 0.86));
        }
        part(sdEllipse(c, vec2(6.4, 5.0)), body);
        part(sdCircle(c - vec2(5.0, 1.8), 2.6), vec3(1.0, 0.82, 0.45));
        eye(c, vec2(5.6, 1.2), 1.0, 0.8, true);
    } else if (kind == 7) {
        // The Brute: a big horned brawler.
        part(sdEllipse(q - vec2(-6.0 - walk * 2.0, -2.2), vec2(5.0, 2.6)), accent * 0.6);
        part(sdEllipse(q - vec2(6.0 + walk * 2.0, -2.2), vec2(5.0, 2.6)), accent * 0.6);
        vec2 c = q - vec2(0.0, -14.0);
        for (int s = -1; s <= 1; s += 2) {
            vec2 v[kMaxPolygonVertices];
            v[0] = vec2(float(s) * 7.0, -9.0);
            v[1] = vec2(float(s) * 13.0, -17.0);
            v[2] = vec2(float(s) * 11.0, -7.0);
            part(sdPolygon(c, v, 3), vec3(0.98, 0.95, 0.85));
        }
        part(sdEllipse(c, vec2(14.0, 12.0)), body);
        fill(sdEllipse(c - vec2(0.0, 5.0), vec2(9.0, 6.0)), accent);
        eye(c, vec2(-4.5, -3.0), 2.6, 0.5, true);
        eye(c, vec2(4.5, -3.0), 2.6, 0.5, true);
        fill(sdSegment(c, vec2(-4.0, 4.0), vec2(4.0, 4.0), 0.9), kInk);
        fill(sdSegment(c, vec2(-3.0, 4.0), vec2(-2.6, 2.4), 0.7), vec3(1.0));
        fill(sdSegment(c, vec2(3.0, 4.0), vec2(2.6, 2.4), 0.7), vec3(1.0));
    } else {
        // The King: a crowned, caped tyrant.
        vec2 c = q - vec2(0.0, -19.0);
        part(sdEllipse(q - vec2(-7.0 - walk * 2.0, -2.4), vec2(5.4, 2.8)), accent * 0.55);
        part(sdEllipse(q - vec2(7.0 + walk * 2.0, -2.4), vec2(5.4, 2.8)), accent * 0.55);
        // Cape behind.
        part(sdRoundBox(q - vec2(-4.0, -14.0), vec2(11.0, 13.0), 6.0), vec3(0.55, 0.08, 0.2));
        part(sdEllipse(c - vec2(0.0, 6.0), vec2(15.0, 13.0)), body);
        fill(sdEllipse(c - vec2(2.0, 10.0), vec2(9.0, 7.0)), accent);
        // Head and crown.
        vec2 h = c - vec2(3.0, -12.0);
        part(sdCircle(h, 9.0), body * 1.05);
        vec2 v[kMaxPolygonVertices];
        v[0] = vec2(-7.0, -6.0);
        v[1] = vec2(-7.0, -15.0);
        v[2] = vec2(-3.5, -10.5);
        v[3] = vec2(0.0, -17.0);
        v[4] = vec2(3.5, -10.5);
        v[5] = vec2(7.0, -15.0);
        v[6] = vec2(7.0, -6.0);
        part(sdPolygon(h, v, 7), vec3(1.0, 0.82, 0.2));
        fill(sdCircle(h - vec2(0.0, -10.0), 1.4), vec3(0.9, 0.15, 0.25));
        eye(h, vec2(-2.4, -1.0), 2.4, 0.8, true);
        eye(h, vec2(4.0, -1.0), 2.4, 0.8, true);
        float grin = max(sdEllipse(h - vec2(1.8, 4.6), vec2(5.0, 2.2)), -(h.y - 4.0));
        part(grin, vec3(0.35, 0.05, 0.08));
        if (angry) {
            fill(max(sdCircle(c, 30.0), -sdCircle(c, 29.0)), vec3(1.0, 0.3, 0.1));
        }
    }
    outColor = acc;
}
