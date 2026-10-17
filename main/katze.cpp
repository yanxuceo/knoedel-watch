#include "katze.h"

#include <initializer_list>
#include <stdio.h>
#include <string.h>

#include "gfx_util.h"
#include "scenery.h"
#include "vec.h"

using namespace gfxu;
using namespace character;
using vec::Path;
using vec::Pen;

namespace katze {

namespace {

constexpr float PI_F = 3.14159265f;
constexpr float DEG = PI_F / 180.0f;

constexpr uint32_t C_BASE = 0xF3A25A, C_EDGE = 0xC9772F, C_BELLY = 0xFFF1DE, C_EAR = 0xF7B9A6;
constexpr uint32_t C_INK = 0x2B2230, C_MOUTH = 0x8C3B3B, C_NOSE = 0xE8807A, C_BLUSH = 0xF49A94;
constexpr uint32_t WHITE = 0xFFFFFF, STAR = 0xFFC93C;
constexpr float EDGE_W = 2.2f;  // outline of the body parts

// The preview draws on a 200 x 200 disc with the feet at (76, 168) and scales
// cat, props and effects by 0.9 around the feet. Slightly larger here; much
// more and the props on the right would leave the disc.
constexpr float FEET_X = 76, FEET_Y = 168, CAT_SCALE = 0.95f;
constexpr float SHOULDER = 24, SHOULDER_Y = -56, ARM = 13.5f;

// Pose parameters. X, Y and TILT are internal; the rest are the registry's
// parameters, in the same order as POSE_NAMES.
constexpr int POSE_PARAMS = 24;
enum {
    X, Y, JUMP, ROT, SX, SY, OPEN, LID, HAPPY, SHUT, LOOKX, LOOKY, SMILE, MOPEN, BLUSH,
    EARL, EARR, TAIL, HLX, HLY, HRX, HRY, FLX, FLY, FRX, FRY,
    TILT,  // of a held watering can, set by the motion "pour"
    P_COUNT
};
static_assert(TILT - JUMP == POSE_PARAMS && POSE_PARAMS <= MAX_POSE, "pose table out of step");

const char *const POSE_NAMES[POSE_PARAMS] = {"jump", "rot", "sx", "sy", "open", "lid", "happy", "shut", "lookX", "lookY",
                                             "smile", "mopen", "blush", "earL", "earR", "tail", "hLx", "hLy",
                                             "hRx", "hRy", "fLx", "fLy", "fRx", "fRy"};

const float NEUTRAL[P_COUNT] = {FEET_X, FEET_Y, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0.25f, 0, 0,
                                0, 0, 0, -28, -31, 28, -31, -12, 0, 12, 0, 0};

float ease(float u) { return -(cosf(PI_F * u) - 1.0f) / 2.0f; }
float fract(float v) { return v - floorf(v); }

// ---- motions: layered on top of the pose; `w` fades them in and out ----------

using MotionFn = void (*)(float *v, float c, float w);

void m_tail_wag(float *v, float c, float w) { v[TAIL] += sinf(c * 3) * 14 * w; }
void m_breathe(float *v, float c, float w)
{
    const float k = 1 + sinf(c * 1.8f) * 0.015f * w;
    v[SX] *= k;
    v[SY] *= k;
}
void m_bounce(float *v, float c, float w)
{
    const float s = fabsf(sinf(c * 5));
    v[JUMP] += w * s * 9;
    v[SY] *= 1 - 0.06f * w * (1 - s);
}
void m_hop(float *v, float c, float w) { v[JUMP] += w * fmaxf(0, sinf(c * 2.4f)) * 3; }
// Walking on the spot: feet step, body bobs.
void m_walk(float *v, float c, float w)
{
    const float s = sinf(c * 6);
    v[JUMP] += fabsf(s) * 2.2f * w;
    v[ROT] += s * 2 * w;
    v[FLY] -= fmaxf(0, s) * 5 * w;
    v[FRY] -= fmaxf(0, -s) * 5 * w;
    v[HLY] += sinf(c * 12) * 1.2f * w;
    v[HRY] += sinf(c * 12) * 1.2f * w;
}
// Small circles of the right paw (sponge, towel).
void m_scrub(float *v, float c, float w)
{
    v[HRX] += cosf(c * 9) * 5 * w;
    v[HRY] += sinf(c * 9) * 4 * w;
    v[ROT] += sinf(c * 9) * 0.8f * w;
}
// Both paws forwards and back along a slight diagonal (vacuum, iron).
void m_push(float *v, float c, float w)
{
    const float d = sinf(c * 3.2f);
    v[HRX] += d * 9 * w;
    v[HRY] += d * 2.5f * w;
    v[HLX] += d * 9 * w;
    v[HLY] += d * 2.5f * w;
    v[ROT] += d * 2 * w;
}
// Wide side-to-side swing of both paws (mop).
void m_sweep(float *v, float c, float w)
{
    const float d = sinf(c * 2.6f);
    v[HRX] += d * 12 * w;
    v[HLX] += d * 12 * w;
    v[ROT] += d * 3 * w;
    v[LOOKX] += d * 0.8f * w;
}
// Gentle smoothing over a surface (bed cover).
void m_smooth(float *v, float c, float w)
{
    const float d = sinf(c * 2.2f);
    v[HLX] += d * 8 * w;
    v[HRX] += d * 8 * w;
    v[ROT] += d * 1.5f * w;
}
// Right paw comes up to the target, holds, goes down again (hanging washing).
void m_reach_up(float *v, float c, float w)
{
    const float u = ease(clampf(sinf(c * 1.8f) * 0.8f + 0.5f));
    v[HRY] += (1 - u) * 22 * w;
    v[HRX] -= (1 - u) * 6 * w;
    v[JUMP] += u * 1.5f * w;
    v[LOOKY] -= u * 0.8f * w;
}
// Tilts the held watering can.
void m_pour(float *v, float c, float w)
{
    const float u = ease(clampf(sinf(c * 1.6f) * 0.9f + 0.55f));
    v[TILT] += u * 34 * w;
    v[HRY] -= u * 2 * w;
}
// Eyes follow a spinning drum; the head tilts along.
void m_watch_spin(float *v, float c, float w)
{
    v[LOOKX] += cosf(c * 4) * 1.6f * w + 0.8f * w;
    v[LOOKY] += sinf(c * 4) * 1.3f * w;
    v[ROT] += sinf(c * 4) * 1.6f * w;
}

// Feet circle round the bike's crank (stage 0, -14); for a pose with jump about 20.
void m_pedal(float *v, float c, float w)
{
    const float a = c * 7, cy = -14 + v[JUMP];
    auto toward = [w](float from, float to) { return from + (to - from) * w; };
    v[FLX] = toward(v[FLX], cosf(a) * 7);
    v[FLY] = toward(v[FLY], cy + sinf(a) * 7);
    v[FRX] = toward(v[FRX], -cosf(a) * 7);
    v[FRY] = toward(v[FRY], cy - sinf(a) * 7);
    v[JUMP] += fabsf(sinf(a)) * w;
}
// Running on the spot: bigger steps, arms swing, the tail streams back.
void m_run(float *v, float c, float w)
{
    const float s = sinf(c * 11);
    v[JUMP] += fabsf(s) * 4 * w;
    v[ROT] += s * 2 * w;
    v[FLX] += s * 6 * w;
    v[FRX] -= s * 6 * w;
    v[FLY] -= fmaxf(0, s) * 7 * w;
    v[FRY] -= fmaxf(0, -s) * 7 * w;
    v[HLX] += s * 7 * w;
    v[HRX] -= s * 7 * w;
    v[HLY] -= fabsf(s) * 3 * w;
    v[HRY] -= fabsf(s) * 3 * w;
    v[TAIL] += (-20 + s * 10) * w;
}
// Impatient foot tapping.
void m_tap(float *v, float c, float w)
{
    v[FRY] -= fmaxf(0, sinf(c * 7)) * 4 * w;
    v[TAIL] += sinf(c * 1.5f) * 8 * w;
}
// Looking left and right; the head tilts, the ears turn.
void m_look_around(float *v, float c, float w)
{
    const float s = sinf(c * 1.7f);
    v[LOOKX] += s * 2.2f * w;
    v[ROT] += s * 3 * w;
    v[EARL] += s * 6 * w;
    v[EARR] -= s * 6 * w;
}
void m_tremble(float *v, float c, float w) { v[X] += sinf(c * 30) * 0.7f * w; }

// a2-einkaufen-verben and a2-termine-verben
void m_type(float *v, float c, float w)
{
    v[HRY] += fmaxf(0, sinf(c * 14)) * 2 * w;
    v[HRX] += sinf(c * 7) * 1.5f * w;
}
// The right paw drops something (a coin into the piggy bank).
void m_drop(float *v, float c, float w)
{
    const float u = ease(clampf(sinf(c * 2) * 0.9f + 0.5f));
    v[HRY] += u * 14 * w;
    v[HRX] += u * 3 * w;
    v[LOOKY] += u * 1.2f * w;
}
// Knocking in bursts.
void m_knock(float *v, float c, float w)
{
    const float on = sinf(c * 1.6f) > -0.2f ? 1 : 0, s = fmaxf(0, sinf(c * 11));
    v[HRX] += s * 4 * w * on;
    v[ROT] += s * 0.6f * w * on;
}
void m_write(float *v, float c, float w)
{
    v[HRX] += sinf(c * 12) * 2.5f * w;
    v[HRY] += sinf(c * 6) * w;
}

// Celebrations: paws up and down in turn, hopping, tail swinging.
void m_dance(float *v, float c, float w)
{
    const float s = sinf(c * 6);
    v[HLY] += s * 14 * w;
    v[HRY] -= s * 14 * w;
    v[JUMP] += fabsf(s) * 4 * w;
    v[ROT] += sinf(c * 3) * 4 * w;
    v[TAIL] += sinf(c * 6) * 16 * w;
}

// a2-gesundheit-nomen: a cough every 1.8 s; eyes shut, mouth open, a small duck.
void m_cough(float *v, float c, float w)
{
    const float u = fmodf(c, 1.8f) / 1.8f, k = u < 0.12f ? u / 0.12f : u < 0.3f ? 1 - (u - 0.12f) / 0.18f : 0;
    v[ROT] += 4 * k * w;
    v[SHUT] = fmaxf(v[SHUT], k * w);
    v[MOPEN] = fmaxf(v[MOPEN], 0.5f * k * w);
    v[JUMP] -= 1.5f * k * w;
}

// a2-wandern-verben and a2-huette-phrasen
void m_climb(float *v, float c, float w)
{
    const float s = sinf(c * 3);
    v[HLY] += s * 5 * w;
    v[HRY] -= s * 5 * w;
    v[FRY] += fmaxf(0, s) * 6 * w;
    v[FLY] -= fmaxf(0, -s) * 6 * w;
    v[JUMP] += fabsf(s) * 1.5f * w;
    v[ROT] += s * 2 * w;
}
// Fanning herself with the right paw.
void m_fan(float *v, float c, float w)
{
    v[HRX] += sinf(c * 12) * 5 * w;
    v[HRY] += cosf(c * 12) * 2 * w;
    v[ROT] += sinf(c * 12) * 0.6f * w;
}
void m_swim(float *v, float c, float w)
{
    v[JUMP] += sinf(c * 2) * 2.4f * w;
    v[HLX] += cosf(c * 4) * 6 * w;
    v[HLY] += sinf(c * 4) * 4 * w;
    v[HRX] -= cosf(c * 4 + PI_F) * 6 * w;
    v[HRY] += sinf(c * 4 + PI_F) * 4 * w;
    v[ROT] += sinf(c * 2) * 2 * w;
}
void m_wave(float *v, float c, float w)
{
    v[HRX] += sinf(c * 8) * 7 * w;
    v[ROT] += sinf(c * 8) * w;
    v[EARR] += sinf(c * 8) * 3 * w;
}

// a2-fitness-verben and a2-sport-draussen
void m_jumping_jack(float *v, float c, float w)
{
    const float s = fabsf(sinf(c * 4));
    v[JUMP] += s * 8 * w;
    v[HLX] -= s * 6 * w;
    v[HLY] -= s * 50 * w;
    v[HRX] += s * 6 * w;
    v[HRY] -= s * 50 * w;
    v[FLX] -= s * 9 * w;
    v[FRX] += s * 9 * w;
}
void m_flex(float *v, float c, float w)
{
    const float s = sinf(c * 3);
    v[HLY] += s * 3 * w;
    v[HRY] += s * 3 * w;
    v[ROT] += s * 2 * w;
}
void m_lift(float *v, float c, float w)
{
    const float u = ease(fabsf(sinf(c * 1.4f)));
    v[HLY] += (1 - u) * 22 * w;
    v[HRY] += (1 - u) * 22 * w;
    v[SY] *= 1 - 0.03f * (1 - u) * w;
    v[X] += sinf(c * 30) * 0.5f * u * w;
}
void m_squat(float *v, float c, float w)
{
    const float s = (1 - cosf(c * 2.6f)) / 2, d = s * 14 * w;
    v[JUMP] -= d;
    v[FLY] -= d;
    v[FRY] -= d;
    v[FLX] -= s * 18 * w;
    v[FRX] += s * 18 * w;
    v[HLY] -= s * 6 * w;
    v[HRY] -= s * 6 * w;
}
void m_skip(float *v, float c, float w)
{
    v[JUMP] += fabsf(sinf(c * 4)) * 12 * w;
    v[HLX] += cosf(c * 8) * 3 * w;
    v[HRX] -= cosf(c * 8) * 3 * w;
}
void m_punch(float *v, float c, float w)
{
    const float u = fmaxf(0, sinf(c * 5)), k = u * u;
    v[HRX] += k * 20 * w;
    v[HRY] -= k * 4 * w;
    v[ROT] += k * 3 * w;
}
void m_curl(float *v, float c, float w)
{
    const float s = (1 - cosf(c * 3)) / 2;
    v[HLY] -= s * 16 * w;
    v[HRY] -= s * 16 * w;
    v[HLX] += s * 4 * w;
    v[HRX] -= s * 4 * w;
}
// Sore muscles: stiff little steps.
void m_stiff(float *v, float c, float w)
{
    const float s = sinf(c * 3);
    v[ROT] += s * 1.5f * w;
    v[FLY] -= fmaxf(0, s) * 2.5f * w;
    v[FRY] -= fmaxf(0, -s) * 2.5f * w;
    v[X] += sinf(c * 30) * 0.5f * w;
}
void m_deep_breath(float *v, float c, float w)
{
    const float s = (1 - cosf(c * 1.4f)) / 2;
    v[SX] *= 1 + s * 0.05f * w;
    v[SY] *= 1 + s * 0.03f * w;
    v[HLY] -= s * 8 * w;
    v[HRY] -= s * 8 * w;
}
void m_pant(float *v, float c, float w)
{
    const float s = fabsf(sinf(c * 7));
    v[SY] *= 1 - s * 0.02f * w;
    v[MOPEN] += s * 0.2f * w;
}
void m_kick(float *v, float c, float w)
{
    const float u = fmodf(c, 1.8f) / 1.8f, k = u < 0.22f ? sinf(u / 0.22f * PI_F) : 0;
    v[FRY] -= k * 14 * w;
    v[FRX] += k * 12 * w;
    v[ROT] -= k * 4 * w;
}
void m_pullup(float *v, float c, float w)
{
    const float s = sinf(c * 2.2f);
    v[JUMP] += s * 8 * w;
    v[HLY] += s * 8 * w;
    v[HRY] += s * 8 * w;
}
void m_wobble(float *v, float c, float w)
{
    v[ROT] += sinf(c * 1.6f) * 3 * w;
    v[X] += sinf(c * 1.6f) * 1.2f * w;
}
void m_ski(float *v, float c, float w)
{
    const float s = sinf(c * 1.3f);
    v[X] += s * 8 * w;
    v[ROT] += s * 5 * w;
}

struct MotionDef {
    const char *id;
    MotionFn fn;
};
const MotionDef MOTIONS[] = {
    {"tail-wag", m_tail_wag}, {"breathe", m_breathe}, {"bounce", m_bounce}, {"hop", m_hop},
    {"walk", m_walk}, {"scrub", m_scrub}, {"push", m_push}, {"sweep", m_sweep},
    {"smooth", m_smooth}, {"reach-up", m_reach_up}, {"pour", m_pour}, {"watch-spin", m_watch_spin},
    {"pedal", m_pedal}, {"run", m_run}, {"tap", m_tap}, {"look-around", m_look_around}, {"tremble", m_tremble},
    {"type", m_type}, {"drop", m_drop}, {"knock", m_knock}, {"write", m_write}, {"dance", m_dance},
    {"cough", m_cough}, {"climb", m_climb}, {"fan", m_fan}, {"swim", m_swim}, {"wave", m_wave},
    {"jumping-jack", m_jumping_jack}, {"flex", m_flex}, {"lift", m_lift}, {"squat", m_squat}, {"skip", m_skip},
    {"punch", m_punch}, {"curl", m_curl}, {"stiff", m_stiff}, {"deep-breath", m_deep_breath}, {"pant", m_pant},
    {"kick", m_kick}, {"pullup", m_pullup}, {"wobble", m_wobble}, {"ski", m_ski},
};
constexpr int MOTION_COUNT = sizeof(MOTIONS) / sizeof(MOTIONS[0]);

// ---- one frame -----------------------------------------------------------------

// A point that a prop publishes for the effects (stage units).
struct Anchor {
    float x, y;
    bool set;
};

// Frames, as in katze.js:
//   stage  the 200 x 200 disc, scaled around the feet
//   scene  origin at the feet: props standing on the stage
//   root   follows the cat's position, jump and squash: feet
//   body   root, turned around the hips: everything else, held props
struct Ctx {
    const Stage *st;
    const float *v;
    float c, w;
    Pen stage, scene, root, body;
    float L[2], R[2];  // paws, body frame
    float head_y, mouth_y, body_y, floor_y;
    Anchor tool, plate, vac, mop, iron, bag, spout, drum, card, phone, wallet, ear, clip, paw_l, paw_r, steam, flash, glass;
    const Anim *anim;
    float pour;  // 0..1 while a can is held, else -1

    void body_to_root(float x, float y, float &ox, float &oy) const
    {
        const float a = v[ROT] * DEG, dy = y + 10;
        ox = x * cosf(a) - dy * sinf(a);
        oy = x * sinf(a) + dy * cosf(a) - 10;
    }
    void root_to_body(float x, float y, float &ox, float &oy) const
    {
        const float a = -v[ROT] * DEG, dy = y + 10;
        ox = x * cosf(a) - dy * sinf(a);
        oy = x * sinf(a) + dy * cosf(a) - 10;
    }
    Anchor root_to_stage(float x, float y) const { return {v[X] + x * v[SX], v[Y] - v[JUMP] + y * v[SY], true}; }
    Anchor body_to_stage(float x, float y) const
    {
        float rx, ry;
        body_to_root(x, y, rx, ry);
        return root_to_stage(rx, ry);
    }
    // Is this stage point on the disc, with room for something of that size?
    bool inside(float x, float y, float margin) const
    {
        const float dx = stage.X(x, y) - st->cx, dy = stage.Y(x, y) - st->cy, r = st->r - margin * stage.k;
        return r > 0 && dx * dx + dy * dy < r * r;
    }
    uint32_t fade(uint32_t color, float a) const { return mix(st->bg, color, a); }
};

// Tapered limb with the body's outline.
void limb(const Pen &p, float x0, float y0, float x1, float y1, float w0, float w1)
{
    p.wedge(x0, y0, x1, y1, w0 + EDGE_W / 2, w1 + EDGE_W / 2, C_EDGE);
    p.wedge(x0, y0, x1, y1, w0 - EDGE_W / 2, w1 - EDGE_W / 2, C_BASE);
}

// Two-bone arm: the paw is pulled in if the target is out of reach; the elbow
// bends outwards.
void reach(float side, float tx, float ty, float *elbow, float *paw)
{
    const float sx = side * SHOULDER, sy = SHOULDER_Y, m = 2 * ARM - 0.5f;
    float dx = tx - sx, dy = ty - sy, d = hypotf(dx, dy);
    if (d > m) {
        tx = sx + dx / d * m;
        ty = sy + dy / d * m;
        d = m;
    }
    if (d < 4) {
        d = 4;
    }
    const float base = atan2f(ty - sy, tx - sx), a = acosf(clampf(d / (2 * ARM), -1, 1));
    const float e1x = sx + ARM * cosf(base + a), e1y = sy + ARM * sinf(base + a);
    const float e2x = sx + ARM * cosf(base - a), e2y = sy + ARM * sinf(base - a);
    const bool first = e1x * side > e2x * side;
    elbow[0] = first ? e1x : e2x;
    elbow[1] = first ? e1y : e2y;
    paw[0] = tx;
    paw[1] = ty;
}

void draw_legs(const Ctx &x)
{
    const float *v = x.v;
    const float feet[2][3] = {{v[FLX], v[FLY], -11}, {v[FRX], v[FRY], 11}};
    for (auto &f : feet) {
        if (f[1] < -3 || fabsf(f[0] - f[2]) > 6) {  // a lifted foot shows its leg
            limb(x.root, f[2], -12, f[0], f[1], 6.5f, 6);
        }
    }
    for (auto &f : feet) {
        x.root.ell(f[0], f[1] - 4, 9, 5.5f, C_BELLY, C_EDGE, EDGE_W);
    }
}

void draw_behind(Ctx &x);  // props behind the torso, after the tail (a backpack)

void draw_body(Ctx &x)
{
    const float *v = x.v;
    const Pen &b = x.body;

    const Pen tail = b.move(20, -16).turn(v[TAIL]);
    Path curl;
    curl.to(0, 0).cubic(26, -2, 30, -32, 18, -46, 9);
    tail.stroke(curl, 10, C_EDGE);
    tail.stroke(curl, 5.6f, C_BASE);
    draw_behind(x);

    for (int side = -1; side <= 1; side += 2) {
        const Pen ear = b.turn(side < 0 ? v[EARL] : -v[EARR], side * 18, -104);
        Path outer, inner;
        outer.to(side * 30, -100).to(side * 27, -134).to(side * 6, -116);
        inner.to(side * 25, -106).to(side * 24, -124).to(side * 13, -114);
        ear.fill(outer, C_BASE, C_EDGE, EDGE_W);
        ear.fill(inner, C_EAR);
    }

    Path torso;
    torso.to(-31, -94).cubic(-35, -60, -32, -22, -20, -9).cubic(-8, 0, 8, 0, 20, -9, 5);
    torso.cubic(32, -22, 35, -60, 31, -94).cubic(27, -122, -27, -122, -31, -94, 9);
    b.fill(torso, C_BASE, C_EDGE, EDGE_W);
    b.ell(0, -34, 18, 20, C_BELLY);
    static const float STRIPES[3][2] = {{-7, -109}, {0, -111}, {7, -109}};
    for (auto &s : STRIPES) {
        b.line(s[0], s[1], s[0], s[1] + 7, 3, C_EDGE);
    }
    b.ell(-20, -77, 5, 2.8f, C_BLUSH, 0.45f + 0.45f * v[BLUSH]);
    b.ell(20, -77, 5, 2.8f, C_BLUSH, 0.45f + 0.45f * v[BLUSH]);

    const float vis = 1.0f - fmaxf(v[HAPPY], v[SHUT]), ry = 4.9f * v[OPEN];
    for (int side = -1; side <= 1; side += 2) {
        const Pen eye = b.move(side * 11, -86);
        if (vis > 0.03f) {
            eye.ell(v[LOOKX], v[LOOKY], 4, ry, C_INK, vis);
            eye.ell(v[LOOKX] + 1.4f, v[LOOKY] - ry * 0.4f, 1.5f, 1.5f, WHITE, vis);
            if (v[LID] > 0.02f) {
                eye.rect(-5.5f, v[LOOKY] - ry - 0.8f, 11, v[LID] * ry * 2 + 0.8f, 0, C_BASE);
            }
        }
        if (v[HAPPY] > 0.03f) {
            Path arc;
            arc.to(-4.5f, 1).quad(0, -5, 4.5f, 1);
            eye.stroke(arc, 2.2f, mix(C_BASE, C_INK, v[HAPPY]));
        }
        if (v[SHUT] > 0.03f) {
            Path arc;
            arc.to(-4.5f, -1).quad(0, 3.5f, 4.5f, -1);
            eye.stroke(arc, 2.2f, mix(C_BASE, C_INK, v[SHUT]));
        }
    }

    Path nose;
    nose.to(-2.6f, -79).to(2.6f, -79).to(0, -76);
    b.fill(nose, C_NOSE, C_NOSE, 1.4f);
    if (v[MOPEN] > 0.02f) {
        Path open;
        open.to(-4.2f, -72.6f).quad(0, -72.6f + 11 * v[MOPEN], 4.2f, -72.6f);
        b.fill(open, C_MOUTH);
    }
    const float smile = -73 + v[SMILE] * 3.6f;
    Path mouth;
    mouth.to(-5.5f, -73).quad(-2.7f, smile, 0, -73.5f, 4).quad(2.7f, smile, 5.5f, -73, 4);
    b.stroke(mouth, 1.7f, C_INK);
    static const float WHISKERS[4][4] = {{-16, -79, -29, -81}, {-16, -75, -29, -73}, {16, -79, 29, -81}, {16, -75, 29, -73}};
    for (auto &q : WHISKERS) {
        b.line(q[0], q[1], q[2], q[3], 1.3f, C_EDGE);
    }
}

// ---- props ---------------------------------------------------------------------
// A prop first publishes the points that effects attach to (`draw` false), and
// is drawn later, in its layer: "back" props stand on the stage behind the cat
// (scene frame), "hand" props are drawn between the arms and the paws (body frame).

using PropFn = void (*)(Ctx &x, bool draw);

void p_bed(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(92, -64, 8, 64, 3, 0xA86B48);
    s.rect(32, -8, 5, 8, 0, 0xA86B48);
    s.rect(30, -30, 66, 22, 4, WHITE, 0xD6D2E0, 1.6f);
    s.ell(83, -36, 10, 6, WHITE, 0xD6D2E0, 1.6f);
    Path cover;
    cover.to(26, -38).to(72, -38).quad(76, -24, 72, -10).to(26, -10).quad(22, -24, 26, -38);
    s.fill(cover, 0xC9C2F2, 0x9D8DF1, 1.6f);
    s.rect(27, -37, 44, 4.5f, 0, mix(0xC9C2F2, WHITE, 0.85f));
}

void p_window(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(40, -116, 52, 58, 3, 0xDDF1FB, 0x8FA3B8, 3);
    s.line(66, -116, 66, -58, 2.4f, 0x8FA3B8);
    s.line(40, -87, 92, -87, 2.4f, 0x8FA3B8);
    s.line(46, -108, 54, -100, 2.4f, WHITE);
    s.line(48, -96, 60, -108, 2.4f, WHITE);
    s.rect(36, -58, 60, 5, 2, 0x8FA3B8);
}

void p_washing_machine(Ctx &x, bool draw)
{
    x.drum = {FEET_X + 69, FEET_Y - 27, true};
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(44, -64, 50, 64, 6, WHITE, 0xB9BFCC, 2);
    s.line(44, -52, 94, -52, 1.6f, 0xB9BFCC);
    s.ell(84, -58, 2.6f, 2.6f, 0x9D8DF1);
    s.rect(50, -60, 14, 4, 2, 0xD6DCE6);
    s.ell(69, -27, 18, 18, 0xDCE3EC, 0x8FA3B8, 3);
    const Pen drum = s.move(69, -27).turn(fmodf(x.c * 260, 360));
    static const uint32_t WASH[3] = {0xE5484D, 0xF2C14E, 0x4BA3D9};
    for (int i = 0; i < 3; i++) {
        const float a = i * 2.1f;
        drum.move(9 * cosf(a), 9 * sinf(a)).turn(a * 57).oval(0, 0, 6, 4, WASH[i]);
    }
    Path water;
    water.to(54, -34).quad(69, -24, 84, -34);
    s.stroke(water, 2, mix(0xDCE3EC, 0x9FC4E8, 0.8f));
}

void p_drying_rack(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(40, 0, 82, -64, 2.4f, 0x9AA4B1);
    s.line(96, 0, 54, -64, 2.4f, 0x9AA4B1);
    s.line(36, -64, 98, -64, 2, 0x9AA4B1);
    s.line(36, -56, 98, -56, 2, 0x9AA4B1);
    // A T-shirt: body and two sleeves, then its outline.
    Path left, right, shirt;
    left.to(58, -64).to(61, -64).to(61, -52).to(54, -54);
    right.to(77, -64).to(80, -64).to(84, -54).to(77, -52);
    shirt.to(58, -64).to(80, -64).to(84, -54).to(78, -52).to(78, -30).to(60, -30).to(60, -52).to(54, -54);
    s.rect(60, -64, 18, 34, 0, 0x9D8DF1);
    s.fill(left, 0x9D8DF1);
    s.fill(right, 0x9D8DF1);
    s.stroke(shirt, 1.4f, 0x7C6BD6, true);
    s.rect(86, -56, 10, 22, 1.5f, 0xF2C14E);
}

void p_ironing_board(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(46, 0, 80, -40, 2.4f, 0x9AA4B1);
    s.line(86, 0, 52, -40, 2.4f, 0x9AA4B1);
    Path board, cloth;
    board.to(28, -40).quad(24, -46, 34, -46, 4).to(94, -46).quad(98, -46, 98, -42, 3).to(98, -40);
    s.fill(board, 0xBFE3F2, 0x7FA6C4, 1.8f);
    cloth.to(60, -46).to(78, -46).to(80, -50).to(72, -52).to(66, -52).to(58, -50);
    s.fill(cloth, WHITE, 0xC9CED6, 1.2f);
}

void p_plant(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    Path pot, leaf_l, leaf_r;
    pot.to(62, -22).to(90, -22).to(86, 0).to(66, 0);
    s.fill(pot, 0xD9824A, 0xB0653A, 1.6f);
    s.line(76, -22, 76, -46, 2.4f, 0x3E8E2F);
    leaf_l.to(76, -30).quad(64, -34, 62, -44).quad(72, -42, 76, -32);
    leaf_r.to(76, -34).quad(88, -38, 90, -48).quad(80, -46, 76, -36);
    s.fill(leaf_l, 0x5E9E45);
    s.fill(leaf_r, 0x5E9E45);
    const Pen flower = s.move(76, -50);
    for (int a = 0; a < 360; a += 72) {
        flower.turn(a).oval(0, -4.5f, 3.2f, 4.5f, 0xF28CA8);
    }
    flower.ell(0, 0, 2.8f, 2.8f, 0xF2C14E);
}

void p_box(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 + 4);
    s.ell(-8, -15, 6, 6, 0xE5484D);
    s.turn(12, 6, -17).rect(2, -24, 8, 14, 1, 0x4BA3D9);
    s.rect(-20, -12, 40, 24, 2, 0xD9A86C, 0xB0803F, 1.6f);
    s.line(-20, -12, -26, -18, 1.6f, 0xB0803F);
    s.line(20, -12, 26, -18, 1.6f, 0xB0803F);
}

void p_sponge(Ctx &x, bool draw)
{
    x.tool = x.body_to_stage(x.R[0] + 4, x.R[1] - 8);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 3, x.R[1] - 5).turn(-20);
    s.rect(-7, -5, 14, 10, 2.5f, 0xF2C14E, 0xC99227, 1.2f);
    s.rect(-6.4f, -4.4f, 12.8f, 3.2f, 1.2f, 0x5E9E45);
}

void p_towel(Ctx &x, bool draw)
{
    x.tool = x.body_to_stage(x.R[0], x.R[1]);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 1, x.R[1] + 2).turn(-10);
    Path cloth;
    cloth.to(-8, -6).to(8, -6).to(9, 10).to(-7, 12);
    s.fill(cloth, WHITE, 0xC9CED6, 1.2f);
    s.line(-7, -1, 7, -1, 1.8f, 0xE5484D);
    s.line(-6.5f, 4, 7.5f, 4, 1.8f, 0xE5484D);
}

void p_plate(Ctx &x, bool draw)
{
    x.plate = x.body_to_stage(x.L[0] + 10, x.L[1] - 4);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.L[0] + 10, x.L[1] - 4);
    s.ell(0, 0, 13, 13, WHITE, 0xB9BFCC, 1.6f);
    s.ell(0, 0, 8, 8, WHITE, 0xDCE3EC, 1.4f);
}

void p_sock(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0], x.R[1] + 2);
    Path outline;
    outline.to(-4, 0).to(4, 0).to(4, 14).quad(4, 18, 10, 18, 3).to(12, 18).quad(16, 18, 16, 22, 3);
    outline.quad(16, 26, 12, 26, 3).to(2, 26).quad(-4, 26, -4, 20, 3);
    s.rect(-4, 0, 8, 22, 0, 0x4BA3D9);       // leg
    s.rect(-4, 18, 20, 8, 4, 0x4BA3D9);      // foot
    s.stroke(outline, 1.2f, 0x2F7DB8, true);
    s.line(-3, 5, 3, 5, 1.6f, WHITE);
    s.line(-3, 9, 3, 9, 1.6f, WHITE);
    s.rect(-2, -6, 4, 8, 1, 0xF2C14E);       // clothes peg
}

void p_iron(Ctx &x, bool draw)
{
    const float px = x.R[0] + 2, py = x.R[1] + 12;
    x.iron = x.body_to_stage(px + 8, py - 6);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(px, py);
    Path body, sole, handle;
    body.to(-12, 4).to(12, 4).quad(14, -4, 6, -8).to(-6, -8).quad(-12, -6, -12, 4);
    s.fill(body, 0xE5484D, 0xB8323A, 1.4f);
    sole.to(-12, 4).to(12, 4).to(13, 6).to(-12, 6);
    s.fill(sole, 0xC9CED6);
    handle.to(-5, -8).quad(-5, -14, 0, -14, 3).to(5, -14).quad(8, -14, 8, -8, 3);
    s.stroke(handle, 2.6f, 0x3B2F4A);
}

void p_trash_bag(Ctx &x, bool draw)
{
    x.bag = x.body_to_stage(x.R[0], x.R[1] + 4);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0], x.R[1] + 2).turn(sinf(x.c * 6) * 6);
    Path bag, fold;
    bag.to(-3, 2).quad(-14, 8, -14, 20).quad(-14, 32, 0, 32).quad(14, 32, 14, 20).quad(14, 8, 3, 2);
    s.fill(bag, 0x4A4F5C, 0x2E323B, 1.4f);
    s.line(-4, 2, -7, -4, 2.2f, 0x2E323B);
    s.line(4, 2, 7, -4, 2.2f, 0x2E323B);
    fold.to(-8, 16).quad(-6, 24, -2, 26, 4);
    s.stroke(fold, 1.6f, 0x6B7180);
}

void p_watering_can(Ctx &x, bool draw)
{
    const float t = x.v[TILT], px = x.R[0] - 2, py = x.R[1] + 18, a = t * DEG;
    x.spout = x.body_to_stage(px + 31 * cosf(a) + 15 * sinf(a), py + 31 * sinf(a) - 15 * cosf(a));
    x.pour = clampf((t - 12) / 18);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(px, py).turn(t);
    Path spout, can, handle;
    spout.to(10, -4).to(30, -16).to(32, -13).to(14, 2);
    s.fill(spout, 0x4BA3D9, 0x2F7DB8, 1.4f);
    s.turn(-30, 31, -15).oval(31, -15, 2.6f, 4, 0x2F7DB8);
    can.to(-12, -10).to(12, -10).to(10, 12).to(-10, 12);
    s.fill(can, 0x4BA3D9, 0x2F7DB8, 1.6f);
    handle.to(-8, -10).quad(0, -22, 8, -10, 6);
    s.stroke(handle, 2.6f, 0x2F7DB8);
}

// Held, but touching the floor: the head is placed on the floor in root coordinates.
void p_vacuum(Ctx &x, bool draw)
{
    float rx, ry, hx, hy;
    x.body_to_root(x.R[0], x.R[1], rx, ry);
    x.root_to_body(rx + 40, -6, hx, hy);
    x.vac = x.root_to_stage(rx + 62, -4);
    if (!draw) {
        return;
    }
    const float px = x.R[0], py = x.R[1];
    const float mx = px + (hx - px) * 0.38f, my = py + (hy - py) * 0.38f, ang = atan2f(hy - py, hx - px) / DEG - 90;
    x.body.line(px, py, hx, hy, 3.6f, 0x8A8FA0);
    x.body.move(mx, my).turn(ang).rect(-5.5f, -10, 11, 20, 4, 0x9D8DF1, 0x7C6BD6, 1.6f);
    x.body.turn(-x.v[ROT], hx, hy).rect(hx - 8, hy - 4.5f, 28, 9, 4, 0x3B2F4A, 0x231C2C, 1.4f);
}

void p_mop(Ctx &x, bool draw)
{
    float rx, ry, hx, hy;
    x.body_to_root(x.R[0], x.R[1], rx, ry);
    x.root_to_body(rx + 30, -10, hx, hy);
    x.mop = x.root_to_stage(rx + 30, -2);
    if (!draw) {
        return;
    }
    x.body.line(x.R[0], x.R[1] - 4, hx, hy, 3.4f, 0xB07A45);
    const Pen head = x.body.move(hx, hy).turn(-x.v[ROT]);
    head.rect(-10, -3, 20, 5, 2, 0x4BA3D9);
    for (int i = 0; i < 5; i++) {
        const float sx = -9 + i * 4;
        head.line(sx, 2, sx - 2 + i, 9, 3, 0x9FC4E8);
    }
}

// ---- a2-unterwegs-verben: on the stage ----

void p_bike(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    const uint32_t INK = 0x3B2F4A, FRAME = 0xE5484D;
    for (float wx : {-40.0f, 44.0f}) {
        const Pen wheel = s.move(wx, -16).turn(fmodf(x.c * 420, 360));
        for (int a = 0; a < 180; a += 60) {
            const Pen spoke = wheel.turn(a);
            spoke.line(-14, 0, 14, 0, 1.2f, 0x9AA4B1);
        }
        s.ring(wx, -16, 16, 3.2f, INK);
    }
    Path tri;
    tri.to(-40, -16).to(0, -14).to(-8, -44);
    s.stroke(tri, 3.4f, FRAME, true);
    s.line(0, -14, 34, -48, 3.4f, FRAME);
    s.line(-8, -44, 34, -48, 3.4f, FRAME);
    s.line(34, -48, 44, -16, 3.4f, FRAME);
    s.rect(-17, -50, 17, 5, 2.5f, INK);
    s.line(34, -48, 38, -60, 3.2f, INK);
    s.line(38, -60, 50, -60, 3.2f, INK);
    s.ell(0, -14, 3.2f, 3.2f, INK);
}

// Drives off to the right and fades away, again and again.
void p_bus(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const float off = fmodf(x.c * 16, 70), a = 1 - clampf((off - 42) / 26);
    const Pen b = x.scene.move(44 + off, 0);
    auto f = [&](uint32_t col) { return x.fade(col, a); };
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 1.2f + i / 3.0f), r = 2.5f + k * 4;
        b.ell(-4 - k * 16, -16 - k * 6, r, r, 0xC9CED6, 0.8f * (1 - k) * a);
    }
    b.rect(0, -74, 84, 62, 9, f(0x4BA3D9), f(0x2F7DB8), 2);
    for (float wx : {6.0f, 28.0f, 50.0f}) {
        b.rect(wx, -66, 18, 18, 3, f(0xDDF1FB));
    }
    b.rect(72, -66, 8, 44, 2, f(0xDDF1FB));
    b.rect(0, -40, 84, 5, 0, f(WHITE));
    b.ell(16, -12, 8, 8, f(0x3B2F4A), f(0x9AA4B1), 2);
    b.ell(66, -12, 8, 8, f(0x3B2F4A), f(0x9AA4B1), 2);
}

void p_bus_stop(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(56, 0, 56, -104, 3.2f, 0x9AA4B1);
    s.ell(56, -112, 13, 13, 0xF2C14E, 0x3E8E2F, 2.6f);
    s.text(x.st->font_small, "H", 56, -112, middle_center, 0x3E8E2F);
    s.rect(48, -88, 16, 22, 2, WHITE, 0x9AA4B1, 1.4f);
    for (float y = -83; y <= -71; y += 4) {
        s.line(51, y, 61, y, 1.2f, 0xC9CED6);
    }
}

void p_train_door(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(34, -128, 96, 122, 9, 0xF3F4F6, 0xB9BFCC, 2);
    s.rect(34, -42, 96, 6, 0, 0xE5484D);
    s.rect(78, -106, 34, 26, 4, 0xBFE3F2);
    s.rect(40, -112, 28, 102, 3, 0x4A4F5C);
    s.rect(44, -108, 20, 40, 2, 0x6B7180);
    s.line(52, -100, 52, -30, 2.4f, 0xD6DCE6);
    s.rect(36, -12, 36, 4, 1.5f, 0x9AA4B1);
}

void p_bus_door(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(30, -120, 100, 108, 10, 0x4BA3D9, 0x2F7DB8, 2);
    s.rect(36, -106, 30, 92, 3, 0x3B4A5C);
    s.rect(74, -106, 34, 28, 4, 0xDDF1FB);
    s.rect(30, -60, 100, 5, 0, WHITE);
    s.ell(100, -12, 9, 9, 0x3B2F4A, 0x9AA4B1, 2);
}

// The text is drawn in the smallest font of the watch, which is larger than in
// the preview: destination above, time and blinking platform below, and the
// later departure left out.
void p_departure_board(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    const uint32_t BOARD = 0x1F3A6E;
    s.line(46, -138, 46, -150, 2, 0x9AA4B1);
    s.line(92, -138, 92, -150, 2, 0x9AA4B1);
    s.rect(34, -138, 72, 40, 4, BOARD);
    s.text(x.st->font_tiny, "München", 39, -127, middle_left, WHITE);
    s.text(x.st->font_tiny, "08:15", 39, -110, middle_left, 0x8EA3C7);
    s.text(x.st->font_tiny, "Gleis 3", 101, -110, middle_right, sinf(x.c * 5) > -0.3f ? 0xFFC93Cu : mix(BOARD, 0xFFC93C, 0.25f));
}

void p_transfer_sign(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(70, -96, 70, -150, 2, 0x9AA4B1);
    s.rect(50, -132, 40, 36, 5, 0x2F5DA8);
    s.line(58, -120, 80, -120, 2.6f, WHITE);
    s.line(74, -126, 80, -120, 2.6f, WHITE);
    s.line(80, -120, 74, -114, 2.6f, WHITE);
    s.line(82, -108, 60, -108, 2.6f, WHITE);
    s.line(66, -114, 60, -108, 2.6f, WHITE);
    s.line(60, -108, 66, -102, 2.6f, WHITE);
    s.rect(54, -92, 32, 13, 2, WHITE);
    s.text(x.st->font_tiny, "ICE", 70, -85.5f, middle_center, 0xE5484D);
}

void p_station_sign(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(44, -132, 44, -146, 2, 0x9AA4B1);
    s.line(96, -132, 96, -146, 2, 0x9AA4B1);
    s.rect(32, -132, 76, 22, 3, 0x2F5DA8);
    s.text(x.st->font_small, "Hamburg", 70, -121, middle_center, WHITE);
}

// ---- a2-unterwegs-verben: held ----

void p_suitcase(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.L[0], x.L[1] + 2);
    Path handle;
    handle.to(-5, 2).to(-5, -3).to(5, -3).to(5, 2);
    s.stroke(handle, 2.2f, 0x3B2F4A);
    s.rect(-12, 2, 24, 19, 3.5f, 0x9D8DF1, 0x7C6BD6, 1.6f);
    s.line(-11, 11, 11, 11, 2.2f, 0xF2C14E);
}

void p_map(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 - 2).turn(-5);
    Path paper, river;
    paper.to(-22, -14).to(-8, -11).to(6, -14).to(22, -11).to(22, 13).to(6, 10).to(-8, 13).to(-22, 10);
    s.fill(paper, 0xFFF6DA, 0xC9B98A, 1.4f);
    s.line(-8, -11, -8, 13, 1.2f, 0xE4D6A8);
    s.line(6, -14, 6, 10, 1.2f, 0xE4D6A8);
    river.to(-22, 2).quad(-10, -6, 0, 2, 4).quad(10, 10, 22, -2, 4);
    s.stroke(river, 2.4f, 0x6FB8E6);
    // The route, dashed.
    static const float ROUTE[4][2] = {{-16, 8}, {-6, 0}, {4, 4}, {12, -6}};
    for (int i = 0; i < 3; i++) {
        const float dx = ROUTE[i + 1][0] - ROUTE[i][0], dy = ROUTE[i + 1][1] - ROUTE[i][1], len = hypotf(dx, dy);
        for (float d = 0; d < len; d += 4.5f) {
            const float e = fminf(d + 2.5f, len);
            s.line(ROUTE[i][0] + dx * d / len, ROUTE[i][1] + dy * d / len, ROUTE[i][0] + dx * e / len,
                   ROUTE[i][1] + dy * e / len, 1.6f, 0xE5484D);
        }
    }
    s.line(10, -9, 15, -4, 1.8f, 0xE5484D);
    s.line(15, -9, 10, -4, 1.8f, 0xE5484D);
}

// A heart, about 16 x 13 units around (0, 0).
Path heart_path()
{
    Path h;
    h.to(0, -2).cubic(-3, -7, -9, -5, -7, 0, 5).to(0, 6).to(7, 0).cubic(9, -5, 3, -7, 0, -2, 5);
    return h;
}

// The word is drawn below the heart, in the watch's smallest font.
void p_welcome_sign(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 + 2);
    s.rect(-22, -14, 44, 28, 3, WHITE, 0xC9CED6, 1.6f);
    s.move(0, -6).scale(0.8f).fill(heart_path(), 0xF06C8B);
    s.text(x.st->font_tiny, "Hallo!", 0, 6, middle_center, 0xC0567A);
}

void p_umbrella(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0], x.R[1]);
    const uint32_t INK = 0x3B2F4A;
    Path hook, canopy;
    hook.to(0, 4).quad(0, 9, -5, 9, 4);
    s.stroke(hook, 2.4f, INK);
    s.line(0, 4, -16, -80, 2.2f, INK);
    canopy.to(-62, -78).cubic(-58, -116, 26, -116, 30, -78, 10);
    const float SCALLOPS[6][4] = {{22, -84, 14, -78}, {7, -84, -1, -78}, {-8, -84, -16, -78},
                                  {-24, -84, -31, -78}, {-39, -84, -46, -78}, {-54, -84, -62, -78}};
    for (auto &q : SCALLOPS) {
        canopy.quad(q[0], q[1], q[2], q[3], 3);
    }
    s.fill(canopy, 0xE5484D, 0xB8323A, 1.6f);
    s.line(-16, -106, -16, -112, 2.2f, INK);
}

// ---- a2-einkaufen-verben and a2-termine-verben ----

// Fills a curved band between two outlines of the same number of points.
void band(const Pen &p, const Path &upper, const Path &lower, uint32_t col)
{
    for (int i = 0; i + 1 < upper.n && i + 1 < lower.n; i++) {
        Path q;
        q.to(upper.p[i][0], upper.p[i][1]).to(upper.p[i + 1][0], upper.p[i + 1][1]);
        q.to(lower.p[i + 1][0], lower.p[i + 1][1]).to(lower.p[i][0], lower.p[i][1]);
        p.fill(q, col);
    }
}

// A shirt on a hanger (clothes rack), centred on x.
void hung_shirt(const Pen &s, float x, uint32_t fill, uint32_t edge)
{
    s.line(x, -98, x, -94, 1.6f, 0x6B7180);
    Path shoulders, outline;
    shoulders.to(x - 11, -86).to(x - 4, -92).to(x + 4, -92).to(x + 11, -86).to(x + 8, -81).to(x - 8, -81);
    outline.to(x - 11, -86).to(x - 4, -92).quad(x, -89, x + 4, -92, 3).to(x + 11, -86).to(x + 8, -81).to(x + 6, -82);
    outline.to(x + 6, -62).to(x - 6, -62).to(x - 6, -82).to(x - 8, -81);
    s.rect(x - 6, -88, 12, 26, 0, fill);
    s.fill(shoulders, fill);
    s.stroke(outline, 1.4f, edge, true);
}

void p_clothes_rack(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(32, 0, 32, -98, 3, 0x9AA4B1);
    s.line(104, 0, 104, -98, 3, 0x9AA4B1);
    s.line(28, -98, 108, -98, 3, 0x9AA4B1);
    hung_shirt(s, 46, 0xE5484D, 0xB8323A);
    hung_shirt(s, 68, 0x4BA3D9, 0x2F7DB8);
    hung_shirt(s, 90, 0xF2C14E, 0xC99227);
}

void p_mirror(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(56, 0, 64, -38, 3, 0xA86B48);
    s.line(80, 0, 72, -38, 3, 0xA86B48);
    s.ell(68, -76, 21, 33, 0xDDF1FB, 0xC99A6B, 4);
    s.line(56, -92, 64, -100, 2.6f, WHITE);
    s.line(57, -82, 70, -95, 2.6f, WHITE);
}

// A shoe standing on (x, y), toe to the right.
void shoe(const Pen &s, float x, float y, uint32_t fill, uint32_t edge)
{
    Path q;
    q.to(x, y).to(x, y - 9).quad(x + 4, y - 11, x + 8, y - 8, 3).to(x + 18, y - 4).quad(x + 22, y - 3, x + 22, y, 3);
    s.fill(q, fill, edge, 1.3f);
}

void p_shoe_shelf(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(40, -108, 64, 108, 3, 0xF3E3CF, 0xC99A6B, 2);
    s.rect(40, -74, 64, 4, 0, 0xC99A6B);
    s.rect(40, -40, 64, 4, 0, 0xC99A6B);
    shoe(s, 46, -74, 0x4BA3D9, 0x2F7DB8);
    shoe(s, 70, -74, 0x4BA3D9, 0x2F7DB8);
    s.rect(46, -76, 46, 2, 0, mix(0xF3E3CF, WHITE, 0.8f));
    shoe(s, 48, -40, 0xB9BFCC, 0x8A8FA0);
    shoe(s, 72, -40, 0xB9BFCC, 0x8A8FA0);
    s.rect(94, -94, 8, 10, 0, 0xF06C8B);
}

void p_card_terminal(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(60, -24, 52, 24, 2, 0xE4DFD6);
    Path stand;
    stand.to(82, -40).to(92, -40).to(94, -24).to(80, -24);
    s.fill(stand, 0x3B2F4A);
    s.rect(74, -78, 26, 38, 4, 0x3B2F4A, 0x231C2C, 1.6f);
    s.rect(78, -74, 18, 13, 2, 0xBFF0D4);
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 3; k++) {
            s.rect(79 + k * 6, -57 + r * 5, 4, 3, 1, 0x6B7180);
        }
    }
}

void p_counter(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(40, -50, 74, 50, 0, 0xD9B48A);
    s.rect(36, -54, 82, 7, 2, 0xA86B48);
    s.stroke(Path::rect(48, -40, 58, 32), 1.6f, 0xC49A6E, true);
}

void p_piggy_bank(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    const uint32_t PINK = 0xF7A8BE, EDGE = 0xD9708E, DARK = 0xB0506C;
    s.rect(52, -26, 34, 6, 2, 0xA86B48);
    s.line(56, -20, 56, 0, 3, 0xA86B48);
    s.line(82, -20, 82, 0, 3, 0xA86B48);
    s.ell(69, -40, 19, 14, PINK, EDGE, 1.6f);
    s.ell(87, -40, 5, 6, PINK, EDGE, 1.4f);
    s.ell(86, -41, 0.9f, 0.9f, DARK);
    s.ell(88.5f, -39, 0.9f, 0.9f, DARK);
    Path ear;
    ear.to(74, -53).to(78, -60).to(80, -51);
    s.fill(ear, PINK, EDGE, 1.2f);
    s.ell(78, -45, 1.6f, 1.6f, C_INK);
    s.rect(62, -55, 10, 2.4f, 1.2f, DARK);
    s.rect(60, -29, 5, 4, 1, PINK, EDGE, 1);
    s.rect(76, -29, 5, 4, 1, PINK, EDGE, 1);
    Path tail;
    tail.to(50, -42).quad(45, -44, 47, -48, 3);
    s.stroke(tail, 1.4f, EDGE);
}

// Worn: a jacket over the body, under the arms.
void p_jacket(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    for (int side = -1; side <= 1; side += 2) {
        Path half, collar;
        half.to(side * 31, -64).quad(side * 36, -36, side * 26, -12, 5).to(side * 5, -10).to(side * 3, -64);
        half.quad(side * 18, -70, side * 31, -64, 4);
        b.fill(half, 0x4A6FA5, 0x33507A, 1.6f);
        collar.to(side * 3, -64).to(side * 12, -50).to(side * 16, -64);
        b.fill(collar, 0x6F8FC2);
    }
    b.ell(-8, -40, 1.4f, 1.4f, 0xF2C14E);
    b.ell(-8, -28, 1.4f, 1.4f, 0xF2C14E);
}

// Worn: a scarf round the neck with one end hanging down.
void p_scarf(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    const uint32_t RED = 0xE5484D, EDGE = 0xB8323A;
    Path upper, lower, outline, end;
    upper.to(-27, -72).quad(0, -60, 27, -72, 8);
    lower.to(-28, -63).quad(0, -51, 28, -63, 8);
    band(b, upper, lower, RED);
    outline.to(-27, -72).quad(0, -60, 27, -72, 8).to(28, -63).quad(0, -51, -28, -63, 8);
    b.stroke(outline, 1.4f, EDGE, true);
    end.to(10, -60).to(19, -62).to(23, -36).to(14, -35);
    b.fill(end, RED, EDGE, 1.4f);
    for (float fx = 15; fx <= 21; fx += 3) {
        b.line(fx, -35 - (fx - 15) / 6, fx, -31 - (fx - 15) / 6, 1.4f, EDGE);
    }
}

void p_shoe_small(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 6, x.R[1] - 12).scale(1.6f);
    Path q;
    q.to(-6, 6).to(-6, -2).quad(-3, -4, 0, -2, 3).to(6, 1).quad(9, 2, 9, 6, 3);
    s.fill(q, 0xE5484D, 0xB8323A, 1.2f);
    s.line(-2, -1, 1, 1, 1, WHITE);
    s.line(0, -2, 3, 0, 1, WHITE);
}

void p_card(Ctx &x, bool draw)
{
    x.card = x.body_to_stage(x.R[0] + 14, x.R[1] - 8);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 6, x.R[1] - 6).turn(-12);
    s.rect(-9, -6, 18, 12, 2, 0x9D8DF1, 0x7C6BD6, 1.4f);
    s.rect(-6, -3, 5, 4, 1, 0xF2C14E);
}

void p_gift(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 + 2);
    s.rect(-16, -12, 32, 22, 2, 0xF2C14E, 0xC99227, 1.4f);
    s.line(0, -12, 0, 10, 3, 0xE5484D);
    s.line(-15, -2, 15, -2, 3, 0xE5484D);
    s.turn(-25, -5, -15).oval(-5, -15, 5, 3.2f, 0xE5484D);
    s.turn(25, 5, -15).oval(5, -15, 5, 3.2f, 0xE5484D);
}

void p_phone(Ctx &x, bool draw)
{
    const float px = (x.L[0] + x.R[0]) / 2, py = fminf(x.L[1], x.R[1]) - 6;
    x.phone = x.body_to_stage(px, py - 16);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(px, py);
    s.rect(-8, -14, 16, 26, 3, 0x3B2F4A, 0x231C2C, 1.2f);
    s.rect(-6, -11, 12, 19, 1.5f, 0xDDF1FB);
    s.line(-3, -6, 3, -6, 1.2f, 0x9AA4B1);
    s.line(-3, -2, 3, -2, 1.2f, 0x9AA4B1);
    s.line(-3, 2, 1, 2, 1.2f, 0x9AA4B1);
}

void p_sweater_small(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 + 4);
    const uint32_t GREEN = 0x5E9E45, EDGE = 0x3E7A2F;
    Path outline;
    outline.to(-12, -8).to(-5, -11).quad(0, -8, 5, -11, 3).to(12, -8).to(15, 0).to(10, 1).to(9, -3).to(9, 10);
    outline.to(-9, 10).to(-9, -3).to(-10, 1).to(-15, 0);
    s.rect(-9, -10, 18, 20, 0, GREEN);
    for (int side = -1; side <= 1; side += 2) {
        Path sleeve;
        sleeve.to(side * 12, -8).to(side * 9, -9).to(side * 9, -3).to(side * 10, 1).to(side * 15, 0);
        s.fill(sleeve, GREEN);
    }
    s.stroke(outline, 1.4f, EDGE, true);
    s.line(-8, 2, 8, 2, 1.4f, 0x8CC56E);
    s.line(-8, 6, 8, 6, 1.4f, 0x8CC56E);
}

void p_toaster(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 - 4);
    s.rect(-14, -10, 28, 20, 6, 0xD6DCE6, 0x8A8FA0, 1.4f);
    s.line(-8, -10, -8, -6, 2.4f, 0x6B7180);
    s.line(0, -10, 0, -6, 2.4f, 0x6B7180);
    s.rect(10, -2, 6, 3, 1, 0x3B2F4A);
    Path smoke;
    smoke.to(-4, -16).quad(-7, -19, -4, -22, 3).quad(-1, -25, -4, -28, 3);
    s.stroke(smoke, 1.6f, 0x8A8FA0);
}

void p_wallet(Ctx &x, bool draw)
{
    x.wallet = x.body_to_stage(x.R[0] + 6, x.R[1] - 2);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 6, x.R[1] - 6).scale(1.3f);
    Path flap;
    flap.to(-12, -2).to(-8, -12).to(16, -12).to(12, -2);
    s.rect(-12, -2, 24, 14, 0, 0xA86B48, 0x7A4A2E, 1.4f);
    s.fill(flap, 0xC08559, 0x7A4A2E, 1.4f);
    s.line(-8, 4, 8, 4, 1.2f, 0x7A4A2E);
}

// A one-euro coin; the € sign is drawn, as no font is that small.
void p_coin(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 2, x.R[1] + 7);
    s.ell(0, 0, 5, 5, 0xF2C14E, 0xC99227, 1.3f);
    Path c;
    for (int a = 50; a <= 310; a += 26) {
        c.to(0.6f + cosf(a * DEG) * 2.6f, sinf(a * DEG) * 2.6f);
    }
    s.stroke(c, 1, 0xB07A10);
    s.line(-2.8f, -0.7f, 0.6f, -0.7f, 0.8f, 0xB07A10);
    s.line(-2.8f, 0.8f, 0.6f, 0.8f, 0.8f, 0xB07A10);
}

void p_phone_ear(Ctx &x, bool draw)
{
    x.ear = x.body_to_stage(x.R[0] + 6, x.R[1] - 16);
    if (!draw) {
        return;
    }
    x.body.move(x.R[0] + 2, x.R[1] - 6).turn(-18).rect(-5, -13, 10, 24, 3, 0x3B2F4A, 0x231C2C, 1.2f);
}

// A wall calendar; the mark shows what happens to the appointment.
enum CalendarMode { CIRCLE, MOVE, CROSS };

void calendar(Ctx &x, bool draw, CalendarMode mode)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(40, -136, 60, 54, 3, WHITE, 0xC9CED6, 1.6f);
    s.rect(40, -136, 60, 12, 3, 0xE5484D);
    s.rect(40, -130, 60, 6, 0, 0xE5484D);
    s.line(52, -140, 52, -131, 2.4f, 0x6B7180);
    s.line(88, -140, 88, -131, 2.4f, 0x6B7180);
    auto cell_x = [](int k) { return 46.0f + k * 13; };
    auto cell_y = [](int r) { return -118.0f + r * 11; };
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 4; k++) {
            s.rect(cell_x(k), cell_y(r), 9, 7, 1, 0xEEF0F4);
        }
    }
    const float u = fmodf(x.c, 3) / 3;
    if (mode == CIRCLE) {
        // Drawn round the date, then held.
        const float cx = cell_x(2) + 4.5f, cy = cell_y(1) + 3.5f, done = clampf(u * 2);
        Path ring;
        for (int i = 0; i <= 24 && i <= done * 24 + 0.5f; i++) {
            ring.to(cx + cosf(i * 15 * DEG) * 8, cy + sinf(i * 15 * DEG) * 6.5f);
        }
        if (ring.n > 1) {
            s.stroke(ring, 2, 0xE5484D);
        }
    } else if (mode == MOVE) {
        // The appointment slides to a later date.
        const float fx = cell_x(1), fy = cell_y(0), tx = cell_x(3), ty = cell_y(2), k = ease(clampf(u * 1.6f - 0.2f));
        Path track;
        track.to(fx + 9, fy + 6).quad(tx - 2, fy + 4, tx + 2, ty - 2, 8);
        for (int i = 0; i + 1 < track.n; i += 2) {
            s.line(track.p[i][0], track.p[i][1], track.p[i + 1][0], track.p[i + 1][1], 1.4f, 0x9AA4B1);
        }
        s.rect(fx + (tx - fx) * k, fy + (ty - fy) * k, 9, 7, 1.5f, 0xF2C14E, 0xC99227, 1);
    } else {
        const float cx = cell_x(1), cy = cell_y(1), a = u < 0.15f ? u / 0.15f : 1;
        const uint32_t col = mix(0xFADBD8, 0xE5484D, a);
        s.rect(cx, cy, 9, 7, 1, 0xFADBD8);
        s.line(cx - 1, cy - 2, cx + 10, cy + 9, 2.4f, col);
        s.line(cx + 10, cy - 2, cx - 1, cy + 9, 2.4f, col);
    }
}
void p_calendar(Ctx &x, bool draw) { calendar(x, draw, CIRCLE); }
void p_calendar_move(Ctx &x, bool draw) { calendar(x, draw, MOVE); }
void p_calendar_x(Ctx &x, bool draw) { calendar(x, draw, CROSS); }

// Worn: a fever thermometer in the mouth.
void p_thermometer(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    b.line(2, -72, 24, -79, 4.4f, 0x9AA4B1);
    b.line(2, -72, 24, -79, 2.8f, WHITE);
    b.line(3, -72.3f, 15, -76.2f, 1.4f, 0xE5484D);
    b.text(x.st->font_tiny, "38°", 32, -88, middle_center, 0xE5484D);
}

void p_big_clock(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.ell(74, -108, 21, 21, WHITE, 0x3B2F4A, 3);
    for (int a = 0; a < 360; a += 90) {
        s.turn(a, 74, -108).line(74, -126, 74, -122, 2, 0x3B2F4A);
    }
    s.turn(fmodf(x.c * 240, 360), 74, -108).line(74, -108, 74, -124, 2.4f, 0x3B2F4A);
    s.turn(fmodf(x.c * 20, 360), 74, -108).line(74, -108, 74, -118, 3, 0xE5484D);
}

// The surgery door. Its sign is a little larger than in the preview, for the
// watch's smallest font.
void p_door(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(50, -128, 54, 128, 3, 0xB07A4E, 0x7A4A2E, 2);
    s.stroke(Path::rect(58, -118, 38, 44, 2), 1.8f, 0x8C5B3A, true);
    s.stroke(Path::rect(58, -64, 38, 52, 2), 1.8f, 0x8C5B3A, true);
    s.ell(60, -66, 3, 3, 0xF2C14E);
    s.rect(59, -110, 36, 14, 2, WHITE);
    s.text(x.st->font_tiny, "Praxis", 77, -103, middle_center, 0x3B2F4A);
}

// The number display of a waiting room, blinking.
void p_number_display(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(70, -132, 70, -142, 2, 0x9AA4B1);
    s.line(96, -132, 96, -142, 2, 0x9AA4B1);
    s.rect(58, -132, 50, 20, 3, 0x2B2230);
    s.text(x.st->font_tiny, "B 042", 83, -122, middle_center, sinf(x.c * 4) > -0.4f ? 0xFF6B5Au : mix(0x2B2230, 0xFF6B5A, 0.35f));
}

// A waiting ticket; its number is a line, as no font is that small.
void p_ticket(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 2, x.R[1]).turn(-8);
    s.rect(-9, -14, 18, 12, 1.5f, WHITE, 0xC9CED6, 1.2f);
    s.line(-5, -8, 5, -8, 2, 0x2B2230);
}

void p_clipboard(Ctx &x, bool draw)
{
    const float px = x.L[0] + 12, py = x.L[1] - 4;
    x.clip = x.body_to_stage(px, py);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(px, py);
    s.rect(-13, -17, 26, 33, 2.5f, 0xC99A6B, 0xA86B48, 1.4f);
    s.rect(-10, -13, 20, 26, 1, WHITE);
    s.rect(-5, -19, 10, 5, 1.5f, 0x9AA4B1);
    for (float y = -9; y <= -1; y += 4) {
        s.line(-7, y, 7, y, 1.4f, 0xC9D3DE);
    }
    s.line(-7, 9, 7, 9, 1, 0x9AA4B1);
}

void p_pen(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0], x.R[1] - 2);
    s.line(0, 0, -8, 9, 2.6f, 0x2F5DA8);
    s.line(-8, 9, -9.5f, 11, 1.6f, 0x2B2230);
}

void p_folder(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2);
    s.turn(-6).rect(-14, -14, 26, 20, 1, WHITE, 0xC9CED6, 1);
    s.rect(-16, -10, 32, 22, 2, 0x4BA3D9, 0x2F7DB8, 1.4f);
    s.rect(-6, -2, 12, 5, 1, WHITE);
}

// ---- a2-wohnung-nomen and a2-gesundheit-nomen (cards drawn by the cat) ----

// An open fridge, its door to the right.
void p_fridge(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(44, -122, 58, 122, 8, 0xF6F8FB, 0xAEB6C4, 2.2f);
    s.rect(48, -78, 50, 74, 3, 0xFFF4C2);
    s.line(49, -56, 97, -56, 2, 0xE9D58A);
    s.line(49, -32, 97, -32, 2, 0xE9D58A);
    s.rect(54, -74, 10, 18, 1.5f, WHITE, 0xC9CED6, 1);
    s.rect(54, -68, 10, 4, 0, 0x4BA3D9);
    s.ell(86, -61, 5, 5, 0xE5484D);
    Path cheese;
    cheese.to(70, -32).to(86, -32).to(86, -42);
    s.fill(cheese, 0xF2C14E);
    s.rect(44, -122, 58, 42, 8, 0xF6F8FB, 0xAEB6C4, 2.2f);
    Path door;
    door.to(102, -80).to(118, -86).to(118, -4).to(102, -2);
    s.fill(door, 0xF6F8FB, 0xAEB6C4, 2.2f);
}

// The back of a sofa behind the cat.
void p_sofa_back(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(-58, -66, 170, 40, 14, 0x9D8DF1, 0x7C6BD6, 2);
    s.rect(-62, -36, 178, 28, 10, 0xB3A6F5, 0x7C6BD6, 2);
    s.rect(96, -50, 22, 44, 10, 0x9D8DF1, 0x7C6BD6, 2);
    s.rect(-50, -8, 5, 8, 0, 0x7A4A2E);
    s.rect(104, -8, 5, 8, 0, 0x7A4A2E);
}

// A cushion hugged in both paws.
void p_pillow(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2).turn(-8);
    s.rect(-14, -14, 28, 28, 8, 0xF2C14E, 0xC99227, 1.6f);
    s.ell(0, 0, 2.4f, 2.4f, 0xC99227);
}

// Worn: a red nose (a cold).
void p_red_nose(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    x.body.ell(0, -77.5f, 4.6f, 3.6f, 0xE5484D);
    x.body.ell(1.4f, -78.8f, 1.1f, 1.1f, 0xFFB3B3);
}

// A tissue in the right paw.
void p_tissue(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] - 2, x.R[1] - 4).turn(-10);
    Path tissue;
    tissue.to(-9, -8).quad(0, -12, 9, -8, 4).to(8, 8).quad(0, 11, -8, 8, 4);
    s.fill(tissue, WHITE, 0xC9CED6, 1.2f);
    Path fold1, fold2;
    fold1.to(-4, -6).quad(-2, 0, -5, 6, 4);
    fold2.to(3, -7).quad(5, 0, 2, 7, 4);
    s.stroke(fold1, 1, 0xE4E7EE);
    s.stroke(fold2, 1, 0xE4E7EE);
}

// Worn: a plaster on the cheek.
void p_plaster_cheek(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.turn(-20, 19, -80);
    s.rect(12, -84, 15, 7, 3, 0xF2D2A6, 0xD9AE7E, 1);
    s.rect(16.5f, -83, 6, 5, 1, 0xE9BF8E);
}

// ---- a2-wandern-verben and a2-huette-phrasen: the mountains ----

Path poly(std::initializer_list<float> xy)
{
    Path q;
    for (auto i = xy.begin(); i != xy.end(); i += 2) {
        q.to(i[0], i[1]);
    }
    return q;
}

// Text that keeps its anchor on the pen, in the stage font nearest its size.
void label(const Ctx &x, const Pen &p, const char *s, float px, float py, float size, uint32_t col,
           lgfx::textdatum_t datum = baseline_center)
{
    p.text(scenery::font_for(*x.st, size * p.k), s, px, py, datum, col);
}

// Backdrops, behind the cat.
void p_mountains(Ctx &x, bool draw)
{
    if (draw) {
        scenery::peaks(x.scene, scenery::DAY);
    }
}

void p_sunset(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.ell(96, -84, 32, 32, x.fade(0xFFD08A, 0.35f));
    s.ell(96, -84, 22 + sinf(x.c * 1.5f) * 1.2f, 22 + sinf(x.c * 1.5f) * 1.2f, x.fade(0xFFB347, 0.95f));
    scenery::peaks(s, scenery::DUSK);
}

void p_hills(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::peaks(s, scenery::DAY);
    Path path;
    path.to(150, -20).quad(100, -12, 70, -6).quad(40, 0, 10, 2);
    s.stroke(path, 6, 0xE8D3A8);
    scenery::fir(s, 100, -16, 1.1f);
    scenery::fir(s, 118, -22, 0.9f, 0x4E9E68);
    scenery::fir(s, -44, -14, 1);
}

void p_slope(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    Path hill;
    hill.to(-120, -8).quad(-40, -16, 20, -50, 8).quad(70, -84, 112, -112, 8).to(132, -118).to(150, -118).to(150, 0).to(-120, 0);
    s.area(hill, 0xB2D896, 0x86BC67, 1.6f);
    Path trail;
    trail.to(-30, -18).quad(14, -26, 2, -36).quad(26, -48, 48, -58).quad(34, -68, 64, -80).quad(84, -92, 104, -108);
    s.stroke(trail, 3.4f, 0xE8D3A8);
    scenery::fir(s, -36, -14, 0.9f);
    scenery::fir(s, 70, -60, 0.8f, 0x4E9E68);
    s.line(118, -114, 118, -146, 3, 0x8A5A3B);
    s.line(110, -138, 126, -138, 3, 0x8A5A3B);
    s.fill(poly({118, -146, 131 + sinf(x.c * 5) * 2, -142 + sinf(x.c * 5 + 1), 118, -138}), 0xE5484D);
}

// In front of the cat: she pulls herself up over its edge.
void p_boulder(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.area(poly({-100, 44, -92, -10, -70, -34, -36, -44, 10, -46, 56, -44, 86, -36, 104, -12, 116, 44}), 0xBDB7AE, 0x8E8880, 2);
    s.area(poly({-70, -34, -36, -44, 10, -46, 56, -44, 86, -36, 74, -30, 40, -36, 0, -38, -40, -36}), 0xD6D1C9);
    s.stroke(poly({-50, -14, -40, 2, -46, 20}), 1.6f, 0x8E8880);
    s.line(60, -20, 68, 0, 1.6f, 0x8E8880);
    s.line(10, -4, 18, 14, 1.6f, 0x8E8880);
    Path grass1, grass2;
    grass1.to(-100, 44).quad(-96, 30, -84, 34, 4).quad(-78, 24, -66, 30, 4).to(-60, 44);
    grass2.to(80, 44).quad(84, 30, 96, 34, 4).quad(104, 28, 112, 44, 4);
    s.area(grass1, 0x7DB35E);
    s.area(grass2, 0x7DB35E);
}

void p_big_sun(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen sun = x.scene.move(80, -112);
    for (int i = 0; i < 10; i++) {
        sun.turn(i * 36 + x.c * 20).line(0, -22, 0, -30, 3, 0xFFB020);
    }
    sun.ell(0, 0, 17, 17, 0xFFC93C, 0xFFB020, 2);
    Path e1, e2, m;
    e1.to(-6, -3).quad(-4, -5, -2, -3, 3);
    e2.to(3, -3).quad(5, -5, 7, -3, 3);
    m.to(-5, 5).quad(0, 9, 5, 5, 4);
    sun.stroke(e1, 1.6f, 0xC77B12);
    sun.stroke(e2, 1.6f, 0xC77B12);
    sun.stroke(m, 1.6f, 0xC77B12);
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.6f + i / 3.0f), a = 0.6f * sinf(PI_F * k);
        if (a < 0.05f) {
            continue;
        }
        Path wisp;
        wisp.to(0, 0).quad(4, -4, 0, -8, 4).quad(-4, -12, 0, -16, 4);
        x.scene.move(48 + i * 22, -20 - k * 20).stroke(wisp, 1.8f, x.fade(0xFFB020, a));
    }
}

void p_bench(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    constexpr uint32_t W = 0xB07A4F, E = 0x8A5A3B;
    scenery::peaks(s, scenery::DAY);
    s.rect(-54, -24, 5, 24, 0, E);
    s.rect(64, -24, 5, 24, 0, E);
    s.rect(-50, -58, 5, 34, 0, E);
    s.rect(70, -58, 5, 34, 0, E);
    s.rect(-64, -56, 146, 7, 2, W, E, 1.2f);
    s.rect(-64, -44, 146, 7, 2, W, E, 1.2f);
    s.rect(-66, -27, 150, 7, 2, W, E, 1.2f);
}

void p_lake_back(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::peaks(s, scenery::DAY);
    s.rect(-120, -44, 280, 90, 0, 0x8FD0F0);
    const uint32_t glint = mix(0x8FD0F0, WHITE, 0.6f);
    s.line(-80, -30, -40, -30, 1.6f, glint);
    s.line(30, -24, 80, -24, 1.6f, glint);
    s.line(-20, -14, 20, -14, 1.6f, glint);
}

void p_night_window(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    constexpr uint32_t SKY = 0x151B3D, WOOD = 0xC89B6D;
    s.rect(40, -116, 52, 58, 3, SKY, WOOD, 3);
    s.ell(76, -100, 8, 8, 0xFFE58A);
    s.ell(80, -103, 7, 7, SKY);
    static const float STARS[4][2] = {{50, -106}, {58, -78}, {84, -72}, {62, -96}};
    for (int i = 0; i < 4; i++) {
        scenery::star(s.move(STARS[i][0], STARS[i][1]).scale(0.35f), mix(SKY, WHITE, 0.4f + 0.6f * fabsf(sinf(x.c * 2 + i * 1.3f))));
    }
    s.line(66, -116, 66, -58, 2.4f, WOOD);
    s.line(40, -87, 92, -87, 2.4f, WOOD);
    s.rect(36, -58, 60, 5, 2, WOOD);
}

// Knödel asleep in the bed, under his own blanket.
void p_knoedel_sleep(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::buddy(s.move(80, -30).scale(0.82f), scenery::SLEEP, x.c, *x.st);
    Path blanket;
    blanket.to(62, -30).quad(74, -34, 98, -30, 4).quad(102, -20, 98, -10, 4).to(62, -10).quad(58, -20, 62, -30, 4);
    s.fill(blanket, 0xC9C2F2, 0x9D8DF1, 1.6f);
    Path fold;
    fold.to(62, -30).quad(74, -34, 98, -30, 4).to(98, -25).quad(74, -29, 62, -25, 4);
    s.area(fold, mix(0xC9C2F2, WHITE, 0.85f));
}

// A small flower nodding, its foot at the origin of `p`.
void flower(const Pen &p, float size, uint32_t col)
{
    p.line(0, 0, 0, 8 * size / 2.6f, 1.4f, 0x5E9E45);
    for (int a = 0; a < 360; a += 72) {
        p.turn(a).oval(0, -size, size * 0.7f, size, col);
    }
    p.ell(0, 0, size * 0.55f, size * 0.55f, 0xF2A93B);
}

// The butterfly of the meadow, its wings beating.
void butterfly(const Pen &p, float c, float size)
{
    Path wings;
    wings.to(0, 0).cubic(-4, -6, -9, -3, -6, 1, 4).cubic(-8, 4, -3, 6, 0, 1, 4).cubic(3, 6, 8, 4, 6, 1, 4).cubic(9, -3, 4, -6, 0, 0, 4);
    p.scale(size, size * (0.6f + 0.4f * fabsf(sinf(c * 12)))).area(wings, 0x9C7AD6);
}

void p_meadow(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::peaks(s, scenery::DAY);
    static const float FLOWERS[7][3] = {{-48, -6, 0xF28CA8}, {-30, -12, 0xFFFFFF}, {86, -10, 0xF2C14E}, {100, -4, 0xF28CA8},
                                        {70, -16, 0xFFFFFF}, {118, -14, 0x9C7AD6}, {-40, -18, 0xF2C14E}};
    for (int i = 0; i < 7; i++) {
        flower(s.move(FLOWERS[i][0], FLOWERS[i][1]).turn(sinf(x.c * 2 + i) * 8), 2.6f, (uint32_t)FLOWERS[i][2]);
    }
    const float k = x.c * 0.7f;
    butterfly(s.move(80 + sinf(k) * 26, -70 + sinf(k * 2.3f) * 10), x.c, 1);
}

void p_summit(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    static const float CLOUDS[2][3] = {{-70, -110, 1}, {104, -60, 0.8f}};
    for (int i = 0; i < 2; i++) {
        const Pen cl = s.move(CLOUDS[i][0] + sinf(x.c * 0.5f + i) * 6, CLOUDS[i][1]).scale(CLOUDS[i][2]);
        const uint32_t col = x.fade(WHITE, 0.9f);
        cl.ell(0, 0, 12, 12, col);
        cl.ell(12, 2, 9, 9, col);
        cl.ell(-11, 3, 8, 8, col);
    }
    s.area(poly({-96, 44, -72, 12, -52, 2, -20, -1, 34, -1, 70, 3, 90, 14, 112, 44}), 0xBDB7AE, 0x8E8880, 2);
    s.area(poly({-52, 2, -20, -1, 34, -1, 70, 3, 60, 8, -40, 8}), 0xD6D1C9);
    s.line(-60, 20, -50, 32, 1.6f, 0x8E8880);
    s.line(40, 18, 52, 30, 1.6f, 0x8E8880);
    s.line(78, 2, 78, -86, 4.5f, 0x8A5A3B);
    s.line(64, -64, 92, -64, 4.5f, 0x8A5A3B);
    s.ell(78, -64, 4, 4, 0xF2C14E);
}

// Trail signs to the hut and the summit; the upper one rocks.
void p_signpost(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(70, -116, 6, 116, 0, 0xA86B48);
    s.rect(70, -44, 6, 4, 0, 0xE5484D);
    s.rect(70, -36, 6, 4, 0, 0xE5484D);
    const auto sign = [&](const Pen &p, float y, const char *place, const char *time) {
        p.fill(poly({28, y, 100, y, 110, y + 9, 100, y + 18, 28, y + 18}), WHITE, 0xC9CED6, 1.2f);
        p.fill(poly({100, y, 110, y + 9, 100, y + 18}), 0xE5484D);
        label(x, p, place, 34, y + 12.5f, 8.5f, 0x2B2230, baseline_left);
        label(x, p, time, 96, y + 12.5f, 8.5f, 0x2B2230, baseline_right);
    };
    sign(s.turn(sinf(x.c * 1.6f) * 1.2f, 73, -101), -110, "Hütte", "1 h");
    sign(s, -86, "Gipfel", "3 h");
}

void p_hut_door(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(24, -132, 130, 132, 0, 0xC68B59);
    for (int y = -124; y < 0; y += 12) {
        s.line(24, y, 154, y, 1.6f, 0xA8703F);
    }
    s.rect(34, -72, 38, 72, 3, 0x8A5A3B, 0x6E4528, 2);
    Path heart;
    heart.to(53, -52).cubic(50, -57, 44, -55, 46, -50, 4).to(53, -44).to(60, -50).cubic(62, -55, 56, -57, 53, -52, 4);
    s.fill(heart, 0xF3D2A6);
    s.ell(66, -34, 2.4f, 2.4f, 0xF2C14E);
    s.rect(84, -102, 34, 30, 0, 0xDDF1FB, 0x6E4528, 2.4f);
    s.line(101, -102, 101, -72, 1.8f, 0x6E4528);
    s.line(84, -87, 118, -87, 1.8f, 0x6E4528);
    s.rect(80, -72, 42, 8, 2, 0x8A5A3B);
    for (int i = 0; i < 5; i++) {
        const float r = 3.6f + sinf(x.c * 3 + i) * 0.4f;
        s.ell(86 + i * 8, -75, r, r, 0xE5484D);
    }
    for (float lx : {88.0f, 98.0f, 108.0f, 116.0f}) {
        s.ell(lx, -71, 3, 1.6f, 0x5E9E45);
    }
    s.rect(30, -94, 46, 14, 3, 0xF3D2A6, 0x8A5A3B, 1.4f);
    label(x, s, "Hütte", 53, -83.5f, 9, 0x6E4528);
}

void p_hut_bench(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    constexpr uint32_t W = 0xB07A4F, E = 0x8A5A3B;
    s.rect(30, -48, 90, 6, 2, W, E, 1.2f);
    s.rect(40, -42, 5, 42, 0, E);
    s.rect(106, -42, 5, 42, 0, E);
    Path mug;
    mug.to(92, -48).to(92, -60).quad(92, -63, 95, -63, 3).to(101, -63).quad(104, -63, 104, -60, 3).to(104, -48);
    s.fill(mug, WHITE, 0xC9CED6, 1.2f);
    Path handle;
    handle.to(104, -58).quad(109, -58, 109, -54, 3).quad(109, -50, 104, -50, 3);
    s.stroke(handle, 1.2f, 0xC9CED6);
    s.rect(26, -27, 100, 6, 2, W, E, 1.2f);
    s.rect(34, -21, 4, 21, 0, E);
    s.rect(114, -21, 4, 21, 0, E);
}

// Today's dish on a blackboard: Speckknödel, with a smiling dumpling.
void p_menu_board(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(54, 0, 66, -110, 3.4f, 0x8A5A3B);
    s.line(102, 0, 90, -110, 3.4f, 0x8A5A3B);
    s.rect(42, -122, 70, 70, 3, 0x2F4A3A, 0xA86B48, 3.4f);
    label(x, s, "Heute", 77, -108, 8.5f, 0xF7D774);
    label(x, s, "Speck-", 77, -95, 10.5f, WHITE);
    label(x, s, "knödel", 77, -83, 10.5f, WHITE);
    const Pen d = s.move(70, -66);
    d.ring(0, 0, 7, 1.3f, WHITE);
    d.ell(-2.6f, -1.5f, 0.8f, 0.8f, WHITE);
    d.ell(2.6f, -1.5f, 0.8f, 0.8f, WHITE);
    Path smile;
    smile.to(-2.6f, 2).quad(0, 4.6f, 2.6f, 2, 3);
    d.stroke(smile, 1.1f, WHITE);
    const Pen fork = d.move(-2, 0);
    fork.line(14, -8, 14, 6, 1.2f, WHITE);
    Path tines;
    tines.to(11, -8).to(11, -2).quad(14, 1, 17, -2, 3).to(17, -8);
    fork.stroke(tines, 1.2f, WHITE);
    label(x, s, "9 €", 98, -61, 8, 0xF7D774);
}

// A fuel pump and the car, Knödel inside; the hose runs to the cat's paw.
void p_car(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(-82, -88, 30, 88, 4, 0xE5484D, 0xB8323A, 1.6f);
    s.rect(-76, -80, 18, 12, 2, WHITE);
    label(x, s, "€", -67, -71, 8, 0x2B2230);
    const float px = x.paw_r.x - FEET_X, py = x.paw_r.y - FEET_Y;
    Path hose;
    hose.to(-58, -60).quad(-60, 10, (px - 58) / 2, 4, 10).quad(px - 6, 0, px - 4, py + 4, 8);
    s.stroke(hose, 3, 0x2B2230);
    Path roof;
    roof.to(30, -14).quad(30, -32, 46, -36, 4).to(62, -56).quad(66, -60, 74, -60, 3).to(122, -60).quad(130, -60, 136, -52, 3);
    roof.to(150, -36).to(150, -14);
    s.area(roof, 0x4BA3D9, 0x2F7DB8, 2);
    s.fill(poly({66, -54, 120, -54, 130, -40, 58, -40}), 0xDDF1FB);
    scenery::buddy(s.move(96, -32).scale(0.62f), scenery::HAPPY, x.c, *x.st);
    Path lower;
    lower.to(54, -40).to(150, -40).to(150, -14).to(30, -14).quad(30, -32, 46, -36, 4);
    s.area(lower, 0x4BA3D9, 0x2F7DB8, 2);
    s.line(92, -54, 92, -40, 2, 0x2F7DB8);
    s.rect(40, -32, 8, 8, 2, 0x2F7DB8);
    s.ell(34, -22, 3, 2.4f, 0xFFE58A);
    for (float wx : {56.0f, 124.0f}) {
        s.ell(wx, -8, 10, 10, 0x3B2F4A);
        s.ell(wx, -8, 4, 4, 0xC9CED6);
    }
}

// An open backpack on the floor, Knödel looking out of it.
void p_backpack_open(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    Path back;
    back.to(40, -2).quad(38, -44, 50, -48, 6).to(82, -48).quad(94, -44, 92, -2, 6).quad(92, 2, 86, 2, 3).to(46, 2).quad(40, 2, 40, -2, 3);
    s.fill(back, 0x4BA3D9, 0x2F7DB8, 2);
    s.ell(66, -47, 16, 4.5f, 0x25486B);
    scenery::buddy(s.move(66, -34).scale(0.72f), scenery::HAPPY, x.c, *x.st);
    Path front;
    front.to(40, -2).quad(39, -34, 44, -42, 5).quad(66, -36, 88, -42, 6).quad(93, -34, 92, -2, 5).quad(92, 2, 86, 2, 3).to(46, 2);
    front.quad(40, 2, 40, -2, 3);
    s.area(front, 0x4BA3D9, 0x2F7DB8, 2);
    s.rect(52, -26, 28, 18, 4, 0x2F7DB8);
    Path flap;
    flap.to(90, -46).quad(104, -52, 108, -40, 4).quad(100, -36, 92, -40, 4);
    s.area(flap, 0x2F7DB8);
    s.turn(-12, 34, -9).rect(26, -14, 16, 10, 2, 0xFFF6DA, 0xC9B98A, 1.2f);
    s.ell(102, -6, 6, 6, 0xE5484D);
    Path stem;
    stem.to(102, -12).quad(103, -15, 105, -16, 3);
    s.stroke(stem, 1.6f, 0x5E9E45);
}

// Held props.
void p_hiking_pole(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 1, x.R[1]);
    s.line(0, 0, 7, 54, 2.2f, 0x6B7180);
    s.stroke(Path::oval(6.2f, 48, 4, 1.4f, 16), 1.2f, 0x3B2F4A, true);
    s.turn(-7).rect(-2.4f, -8, 5, 11, 2.2f, 0xE5484D);
}

void p_water_bottle(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 1, x.R[1] + 4).turn(8);
    s.rect(-4.5f, -16, 9, 20, 3.5f, 0xE5484D, 0xB8323A, 1.2f);
    s.rect(-3, -20, 6, 5, 1, 0xC9CED6);
    s.rect(-4.5f, -8, 9, 4, 0, mix(0xE5484D, WHITE, 0.8f));
}

void p_fuel_nozzle(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0], x.R[1]);
    s.fill(poly({-5, -4, 5, -4, 7, 4, -3, 6}), 0x2B2230);
    s.line(5, -2, 15, 2, 3, 0x6B7180);
    s.rect(-6, -7, 8, 4, 1.5f, 0xF2C14E);
}

void p_camera(Ctx &x, bool draw)
{
    const float cx = (x.L[0] + x.R[0]) / 2, cy = (x.L[1] + x.R[1]) / 2;
    x.flash = x.body_to_stage(cx + 16, cy - 6);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(cx, cy - 2);
    s.rect(-12, -8, 22, 15, 3, 0x3B3F4A, 0x22252C, 1.2f);
    s.rect(-8, -11, 7, 4, 1, 0x3B3F4A);
    s.rect(8, -6, 7, 11, 2, 0x55596A, 0x22252C, 1);
    s.ell(15.5f, -0.5f, 2, 4.5f, 0x7FB6E0);
    s.ell(-7, -3, 1.5f, 1.5f, 0xE5484D);
}

void p_fork(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 1, x.R[1] - 2).turn(10);
    s.line(0, 4, 0, -12, 1.8f, 0x9AA4B1);
    Path tines;
    tines.to(-3, -20).to(-3, -12).quad(0, -9, 3, -12, 3).to(3, -20);
    s.stroke(tines, 1.8f, 0x9AA4B1);
    s.line(0, -20, 0, -12, 1.8f, 0x9AA4B1);
    s.fill(poly({-4, -26, 4, -27, 5, -21, -3, -20}), 0xE8B04B, 0xC08A2E, 0.8f);
}

void p_glass(Ctx &x, bool draw)
{
    x.glass = x.body_to_stage(x.R[0] + 6, x.R[1] - 22);
    if (!draw) {
        return;
    }
    const Pen s = x.body.move(x.R[0] + 1, x.R[1] - 3).turn(14);
    s.fill(poly({-5, -20, 5, -20, 4, 2, -4, 2}), 0xF7C948, 0xC9A227, 1.2f);
    s.rect(-5.4f, -24, 10.8f, 5, 2, WHITE);
    for (int i = 0; i < 3; i++) {
        s.ell(-2 + i * 2, -1 - fract(x.c * 0.8f + i / 3.0f) * 16, 0.9f, 0.9f, mix(0xF7C948, WHITE, 0.8f));
    }
}

void p_knoedel_hug(Ctx &x, bool draw)
{
    if (draw) {
        scenery::buddy(x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2 + 18).scale(0.95f), scenery::HUG, x.c, *x.st);
    }
}

// Worn.
void p_nightcap(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    Path cap;
    cap.to(-24, -110).quad(-20, -138, 6, -140, 5).quad(30, -140, 42, -116, 5).quad(46, -106, 40, -100, 3);
    cap.quad(34, -122, 18, -126, 5).quad(22, -116, 24, -106, 3);
    b.area(cap, 0x9C7AD6, 0x7A5AC8, 1.6f);
    Path band;
    band.to(-26, -108).quad(0, -118, 26, -108, 6).to(26, -102).quad(0, -112, -26, -102, 6);
    b.area(band, WHITE, 0xD6D2E0, 1.2f);
    b.ell(41, -98 + sinf(x.c * 1.8f) * 1.2f, 5, 5, WHITE, 0xD6D2E0, 1.2f);
    static const float STARS[3][2] = {{-10, -126}, {10, -130}, {26, -120}};
    for (auto &st : STARS) {
        scenery::star(b.move(st[0], st[1]).scale(0.4f), 0xFFE58A);
    }
}

void p_sunglasses(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    for (int side = -1; side <= 1; side += 2) {
        Path lens;
        lens.to(side * 21, -91).to(side * 2, -91).to(side * 3, -83).quad(side * 6, -79, side * 12, -79, 3);
        lens.quad(side * 19, -79, side * 21, -85, 3);
        b.fill(lens, 0x2B2230);
        b.line(side * 21, -90, side * 30, -92, 1.8f, 0x2B2230);
    }
    Path bridge;
    bridge.to(-2, -89).quad(0, -91, 2, -89, 3);
    b.stroke(bridge, 1.8f, 0x2B2230);
    const uint32_t shine = mix(0x2B2230, WHITE, 0.55f);
    b.line(-17, -88, -12, -88, 1.4f, shine);
    b.line(6, -88, 11, -88, 1.4f, shine);
}

void p_backpack_straps(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    for (int side = -1; side <= 1; side += 2) {
        Path strap;
        strap.to(side * 17, -110).quad(side * 24, -80, side * 23, -46, 6);
        b.stroke(strap, 5, 0x25486B);
    }
    b.line(-21, -72, 21, -72, 2.4f, 0x25486B);
    b.rect(-4, -75, 8, 6, 1.5f, 0xF2C14E);
}

// Behind the torso.
void p_backpack(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    Path pack;
    pack.to(-32, -40).quad(-36, -130, -26, -146, 8).quad(0, -158, 26, -146, 6).quad(36, -130, 32, -40, 8);
    b.fill(pack, 0x4BA3D9, 0x2F7DB8, 2);
    Path lid;
    lid.to(-27, -144).quad(0, -156, 27, -144, 6).to(28, -132).quad(0, -142, -28, -132, 6);
    b.area(lid, 0x2F7DB8);
    b.rect(-42, -84, 12, 26, 4, 0x2F7DB8);
    b.rect(30, -84, 12, 26, 4, 0x2F7DB8);
    b.rect(34, -98, 5, 16, 2, 0xE5484D);
}

void p_knoedel_on_pack(Ctx &x, bool draw)
{
    if (draw) {
        scenery::buddy(x.body.move(2, -149).scale(0.82f), scenery::WOW, x.c, *x.st);
    }
}

// In front of the cat: lake water up to her chest, rings around her.
void p_lake_water(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    constexpr uint32_t WATER = 0x7CC3EA;
    Path water, crest;
    water.to(-120, 50);
    for (int wx = -120; wx <= 160; wx += 10) {
        const float wy = -34 + sinf(wx * 0.08f + x.c * 2.4f) * 1.8f;
        water.to(wx, wy);
        crest.to(wx, wy);
    }
    water.to(160, 50);
    s.area(water, x.fade(WATER, 0.94f));
    s.stroke(crest, 2, mix(WATER, WHITE, 0.8f));
    const float cx = x.v[X] - FEET_X;
    for (int i = 0; i < 2; i++) {
        const float k = fract(x.c * 0.5f + i / 2.0f), a = 0.7f * (1 - k);
        if (a > 0.05f) {
            s.stroke(Path::oval(cx, -34, 26 + k * 26, 3 + k * 3, 28), 1.6f, mix(WATER, WHITE, a), true);
        }
    }
}

void p_table_food(Ctx &x, bool draw)
{
    x.steam = {FEET_X + 2, FEET_Y - 50, true};
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::table_cloth(s);
    s.ell(2, -38, 22, 5.5f, WHITE, 0xC9CED6, 1.4f);
    static const float PIECES[4][5] = {{-10, -42, 9, 5, -10}, {2, -44, 10, 5, 8}, {12, -41, 8, 4.5f, -6}, {-2, -40, 8, 4, 4}};
    for (auto &q : PIECES) {
        s.turn(q[4], q[0], q[1]).rect(q[0] - q[2] / 2, q[1] - q[3] / 2, q[2], q[3], 1.6f, 0xE8B04B, 0xC08A2E, 0.8f);
    }
    static const float SALT[5][2] = {{-6, -46}, {4, -47}, {10, -44}, {-12, -44}, {0, -42}};
    for (auto &q : SALT) {
        s.ell(q[0], q[1], 0.9f, 0.9f, WHITE);
    }
    s.ell(64, -38, 13, 4, WHITE, 0xC9CED6, 1.2f);
    Path bowl;
    bowl.to(53, -40).quad(64, -30, 75, -40, 4);
    s.fill(bowl, 0xF2C14E, 0xC99227, 1);
}

void p_table_drinks(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::table_cloth(s);
    Path pretzel;
    pretzel.to(-8, 2).quad(-12, -8, -4, -9, 4).quad(0, -3, 4, -9, 4).quad(12, -8, 8, 2, 4).quad(4, -2, 0, 2, 3).quad(-4, -2, -8, 2, 3);
    s.move(16, -40).stroke(pretzel, 3.2f, 0xB0653A, true);
}

void p_table_empty(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    scenery::table_cloth(s);
    s.ell(4, -38, 18, 4.6f, WHITE, 0xC9CED6, 1.3f);
    s.ell(64, -38, 13, 4, WHITE, 0xC9CED6, 1.2f);
    const Pen bill = s.move(34, -42).turn(-8);
    bill.rect(-7, -10, 14, 16, 1.5f, WHITE, 0xC9CED6, 1.2f);
    bill.line(-4, -6, 4, -6, 1.1f, 0x9AA4B1);
    bill.line(-4, -3, 4, -3, 1.1f, 0x9AA4B1);
    bill.line(-4, 0, 2, 0, 1.1f, 0x9AA4B1);
}

// ---- a2-fitness-verben and a2-sport-draussen ----

// A gym: a pale wall stripe, the floor, a clock on the wall.
void p_gym(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(-120, -64, 280, 5, 0, x.fade(WHITE, 0.55f));
    s.rect(-120, 0, 280, 50, 0, 0xC9CED6);
    s.rect(-120, 0, 280, 3, 0, 0x9AA4B1);
    const Pen clock = s.move(-62, -112);
    clock.ell(0, 0, 11, 11, WHITE, 0x6B7180, 2);
    clock.turn(fmodf(x.c * 60, 360)).line(0, 0, 0, -8, 1.6f, 0x2B2230);
    clock.line(0, 0, 5, 0, 1.8f, 0x2B2230);
}

void p_headband(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    Path band;
    band.to(-30.5f, -104).quad(0, -115, 30.5f, -104, 6).to(30.5f, -97).quad(0, -108, -30.5f, -97, 6);
    b.area(band, 0xE5484D);
    Path stripe;
    stripe.to(-30.5f, -100.5f).quad(0, -111.5f, 30.5f, -100.5f, 6);
    b.stroke(stripe, 1.3f, WHITE);
    const float a = sinf(x.c * 6) * 3;
    b.fill(poly({29, -103, 44, -100 + a, 42, -95 + a, 29, -98}), 0xE5484D);
}

// A barbell across both paws. Its plates' "25" is left out: no font is that small.
void p_barbell(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const float a = atan2f(x.R[1] - x.L[1], x.R[0] - x.L[0]) * 180 / PI_F;
    const Pen s = x.body.move((x.L[0] + x.R[0]) / 2, (x.L[1] + x.R[1]) / 2).turn(a);
    s.line(-58, 0, 58, 0, 3.2f, 0x6B7180);
    for (int k = -1; k <= 1; k += 2) {
        s.rect(k * 44 - 4, -16, 8, 32, 2, 0x3B3F4A);
        s.rect(k * 51 - 2.5f, -11, 5, 22, 1.5f, 0xE5484D);
        s.rect(k * 38 - 1.5f, -5, 3, 10, 1, 0x9AA4B1);
    }
}

void p_treadmill(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(58, -10, 66, -84, 5, 0x9AA4B1);
    s.line(64, -66, 24, -62, 4, 0x9AA4B1);
    s.rect(46, -100, 40, 18, 3, 0x2B2230);
    label(x, s, "8 km/h", 66, -88, 7, 0x5BE38A);
    s.fill(poly({-58, -9, 66, -9, 72, 0, -62, 0}), 0x3B3F4A);
    for (int i = 0; i < 7; i++) {
        s.rect(-54 + fmodf(fmodf(i * 18 - x.c * 70, 126) + 126, 126), -9, 2, 3, 0, 0x6B7180);
    }
}

// The rope swings from paw to paw, over the head and under the feet.
void p_jump_rope(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const float lx = x.paw_l.x - FEET_X, ly = x.paw_l.y - FEET_Y, rx = x.paw_r.x - FEET_X, ry = x.paw_r.y - FEET_Y;
    const float k = -cosf(x.c * 8), dy = k > 0 ? k * 120 : k * 190;
    Path rope;
    rope.to(lx, ly).quad((lx + rx) / 2, (ly + ry) / 2 + dy, rx, ry, 12);
    x.scene.stroke(rope, 2.4f, 0xE5484D);
}

void p_punching_bag(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.line(40, -152, 96, -152, 4, 0x6B7180);
    const float hit = fmaxf(0, sinf(x.c * 5 - 0.7f));
    const Pen bag = s.turn(hit * hit * 9, 62, -152);
    bag.line(62, -152, 62, -124, 2, 0x6B7180);
    bag.rect(46, -124, 32, 86, 12, 0xC0392B, 0x8E2A20, 2);
    bag.rect(46, -112, 32, 5, 0, 0x2B2230);
    bag.rect(46, -54, 32, 5, 0, 0x2B2230);
    bag.rect(51, -104, 4, 42, 2, mix(0xC0392B, WHITE, 0.3f));
    if (fmaxf(0, sinf(x.c * 5)) > 0.97f) {
        scenery::star(s.move(48, -72).scale(1.4f), STAR);
    }
}

void glove(const Pen &s)
{
    s.rect(-6, 5, 12, 6, 2, WHITE, 0xC9CED6, 1);
    s.ell(0, 0, 9, 8.5f, 0xE5484D, 0xB8323A, 1.6f);
    s.ell(-6, 2, 3.4f, 4, 0xE5484D, 0xB8323A, 1.2f);
    s.ell(2, -3, 3, 1.6f, mix(0xE5484D, WHITE, 0.4f));
}

void p_boxing_gloves(Ctx &x, bool draw)
{
    if (draw) {
        glove(x.body.move(x.L[0], x.L[1]));
        glove(x.body.move(x.R[0], x.R[1]).scale(-1, 1));
    }
}

void p_mat(Ctx &x, bool draw)
{
    if (draw) {
        x.scene.fill(poly({-62, -3, 74, -3, 82, 6, -70, 6}), 0x9C7AD6);
        x.scene.line(-62, -3, 74, -3, 1.6f, 0x7A5AC8);
    }
}

void p_yoga_mat(Ctx &x, bool draw)
{
    if (draw) {
        x.scene.fill(poly({-62, -3, 80, -3, 88, 6, -70, 6}), 0x4FB8A0);
        x.scene.line(-62, -3, 80, -3, 1.6f, 0x2E9A7A);
    }
}

void dumbbell(const Pen &s)
{
    s.line(-8, 0, 8, 0, 2.6f, 0x6B7180);
    s.rect(-11.5f, -6.5f, 5, 13, 1.6f, 0x3B3F4A);
    s.rect(6.5f, -6.5f, 5, 13, 1.6f, 0x3B3F4A);
}

void p_dumbbells(Ctx &x, bool draw)
{
    if (draw) {
        dumbbell(x.body.move(x.L[0], x.L[1]));
        dumbbell(x.body.move(x.R[0], x.R[1]));
    }
}

// A plank timer counting down from 0:20 once per loop.
void p_timer(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(34, -136, 66, 30, 5, 0x2B2230, 0x6B7180, 2);
    const int n = (int)fmaxf(0, 20 - floorf(fmodf(x.c, LOOP_SECONDS) / LOOP_SECONDS * 21));
    char text[8];
    snprintf(text, sizeof(text), "0:%02d", n);
    label(x, s, text, 67, -114.5f, 16, 0xFF5A5A);
}

void p_scale(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    s.rect(-36, -9, 72, 10, 3, WHITE, 0xB9BFCC, 1.6f);
    s.rect(-9, -7, 18, 5, 1, 0xDDF1FB);
    const Pen lab = s.move(66, -76 - fabsf(sinf(x.c * 2.4f)) * 5);
    lab.rect(-22, -10, 44, 20, 10, WHITE, 0x2E9A7A, 1.8f);
    label(x, lab, "−5 kg", 0, 5, 12, 0x2E9A7A);
}

void p_park(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    Path hill, lawn;
    hill.to(-120, -22).quad(-60, -46, 0, -28, 8).quad(60, -12, 150, -36, 8).to(150, 0).to(-120, 0);
    s.area(hill, 0xB2D896);
    s.rect(-120, -2, 280, 52, 0, 0x93C474);
    lawn.to(-120, -6).quad(-40, -16, 30, -8, 8).quad(90, 0, 150, -10, 8).to(150, 0).to(-120, 0);
    s.area(lawn, 0x93C474);
    scenery::tree(s, -58, -16, 1.1f);
    scenery::tree(s, 104, -22, 1.2f, 0x4E9E68);
    s.ell(84, -8, 8, 8, 0x5BAF74);
    s.ell(92, -6, 6, 6, 0x4E9E68);
}

void p_field(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    Path back;
    back.to(-120, -30).quad(0, -40, 150, -28, 8).to(150, 0).to(-120, 0);
    s.area(back, 0xC9DE9A);
    s.rect(-120, -2, 280, 52, 0, 0xA8C97A);
    scenery::tree(s, -60, -26, 0.9f, 0xF2A93B);
    scenery::tree(s, -40, -28, 0.7f, 0xE5873A);
    static const float TUFTS[6] = {-30, -10, 20, 40, 70, 96};
    for (int i = 0; i < 6; i++) {
        const float tx = TUFTS[i], a = sinf(x.c * 3 + i) * 3;
        Path blade;
        blade.to(tx, 0).quad(tx + 1, -5, tx + 3 + a, -9, 4);
        s.stroke(blade, 1.6f, 0x7DA855);
        blade.n = 0;
        blade.to(tx + 4, 0).quad(tx + 5, -4, tx + 8 + a, -6, 4);
        s.stroke(blade, 1.6f, 0x7DA855);
    }
}

// A football: white with a black pentagon and seams.
void ball(const Pen &s, float r, float scale_pent, float seam)
{
    s.ell(0, 0, r, r, WHITE, 0x2B2230, 1.4f);
    const float k = scale_pent;
    s.fill(poly({0, -3.4f * k, 3.2f * k, -1 * k, 2 * k, 2.8f * k, -2 * k, 2.8f * k, -3.2f * k, -1 * k}), 0x2B2230);
    if (seam > 0) {
        s.line(0, -3.4f, 0, -8.6f, seam, 0x2B2230);
        s.line(3.2f, -1, 8, -2.6f, seam, 0x2B2230);
        s.line(2, 2.8f, 5, 7, seam, 0x2B2230);
        s.line(-2, 2.8f, -5, 7, seam, 0x2B2230);
        s.line(-3.2f, -1, -8, -2.6f, seam, 0x2B2230);
    }
}

void p_ball(Ctx &x, bool draw)
{
    if (draw) {
        ball(x.body.move(x.R[0] + 4, x.R[1] - 6), 9, 1, 1);
    }
}

// A goal with Knödel as the startled keeper.
void p_goal(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    const uint32_t net = x.fade(WHITE, 0.7f);
    for (int nx = 52; nx <= 126; nx += 9) {
        s.line(nx, -62, nx + 4, -2, 1, net);
    }
    for (int ny = -54; ny <= -6; ny += 9) {
        s.line(48, ny, 130, ny, 1, net);
    }
    scenery::buddy(s.move(90, 0), scenery::SHOCK, x.c, *x.st);
    s.stroke(poly({46, 0, 46, -66, 134, -66, 134, 0}), 4.5f, WHITE);
    s.rect(-120, 0, 280, 50, 0, 0x93C474);
    s.line(-120, 0, 160, 0, 2, mix(0x93C474, WHITE, 0.7f));
}

// The ball flies from the cat's foot towards the goal.
void p_football(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const float u = fmodf(x.c, 1.8f) / 1.8f;
    const Pen s = x.scene.move(24 + u * 62, -9 - sinf(u * PI_F) * 36).turn(u * 540);
    s.ell(0, 0, 7.5f, 7.5f, WHITE, 0x2B2230, 1.3f);
    s.fill(poly({0, -2.8f, 2.7f, -0.8f, 1.7f, 2.3f, -1.7f, 2.3f, -2.7f, -0.8f}), 0x2B2230);
}

void p_pullup_bar(Ctx &x, bool draw)
{
    if (draw) {
        x.scene.rect(-64, -104, 5, 104, 0, 0x6B7180);
        x.scene.rect(59, -104, 5, 104, 0, 0x6B7180);
        x.scene.line(-62, -101, 62, -101, 4, 0x9AA4B1);
    }
}

// A kite with Knödel on it; its string runs to the cat's paw.
__attribute__((noinline)) void kite_body(Ctx &x, const Pen &k)
{
    Path tail;
    tail.to(0, 24);
    for (int i = 1; i <= 6; i++) {
        tail.to(-i * 4 + sinf(x.c * 4 + i) * 3, 24 + i * 6);
    }
    k.stroke(tail, 1.2f, 0x3B2F4A);
    static const uint32_t BOWS[3] = {0xE5484D, 0x4BA3D9, 0xF2C14E};
    for (int i = 0; i < 3; i++) {
        const int j = i * 2 + 2;
        const Pen bow = k.move(-j * 4 + sinf(x.c * 4 + j) * 3, 24 + j * 6);
        bow.line(-3, -2, 3, 2, 2.6f, BOWS[i]);
        bow.line(-3, 2, 3, -2, 2.6f, BOWS[i]);
    }
    k.fill(poly({0, -20, 15, 0, 0, 24}), 0xE5484D);
    k.fill(poly({0, -20, -15, 0, 0, 24}), 0xF2C14E);
    k.line(0, -20, 0, 24, 1.4f, 0x8A5A3B);
    k.line(-15, 0, 15, 0, 1.4f, 0x8A5A3B);
    scenery::buddy(k.move(0, 8).scale(0.48f), scenery::WOW, x.c, *x.st);
}

void p_kite(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const float kx = 78 + sinf(x.c * 0.9f) * 8, ky = -140 + sinf(x.c * 1.3f) * 6;
    const float px = x.paw_r.x - FEET_X, py = x.paw_r.y - FEET_Y;
    Path string;
    string.to(kx, ky + 22).quad((kx + px) / 2 + 10, (ky + py) / 2 + 12, px, py, 10);
    x.scene.stroke(string, 1, 0x3B2F4A);
    kite_body(x, x.scene.move(kx, ky).turn(sinf(x.c * 1.1f) * 8));
}

// Under water: light from above, sand, swaying weeds, two fish.
void p_underwater(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    static const float RAYS[3][2] = {{-50, 0.5f}, {30, 0.4f}, {96, 0.45f}};
    for (auto &r : RAYS) {
        s.fill(poly({r[0], -170, r[0] + 24, -170, r[0] + 6, 10, r[0] - 14, 10}), x.fade(WHITE, r[1] * 0.4f));
    }
    Path sand;
    sand.to(-120, -4).quad(-60, -12, 0, -4, 8).quad(60, 4, 150, -6, 8).to(150, 50).to(-120, 50);
    s.area(sand, 0xF2DFB0);
    static const float WEEDS[4][2] = {{-46, 0x3E8E5A}, {-36, 0x5BAF74}, {92, 0x3E8E5A}, {100, 0x5BAF74}};
    for (int i = 0; i < 4; i++) {
        const float wx = WEEDS[i][0], a = sinf(x.c * 1.6f + i) * 6;
        Path weed;
        weed.to(wx, 0).quad(wx + a, -18, wx, -34, 6).quad(wx - a, -48, wx + a * 0.6f, -56 - (i % 2) * 10, 6);
        s.stroke(weed, 4, (uint32_t)WEEDS[i][1]);
    }
    s.ell(70, -2, 7, 4, 0xF49A94);
    s.fill(poly({-14, -2, -11, -8, -8, -2}), 0xE5873A);
    for (int i = 0; i < 2; i++) {
        const float u = fract(x.c * (i ? 0.09f : 0.12f) + i * 0.5f);
        const Pen f = s.move(140 - u * 260, (i ? -120 : -84) + sinf(x.c * 3 + i) * 3);
        f.fill(poly({8, 0, 16, -6, 16, 6}), i ? 0x4BA3D9 : 0xF2A93B);
        f.ell(0, 0, 10, 6, i ? 0x6FB8E6 : 0xF7C948);
        f.ell(-5, -1.5f, 1.4f, 1.4f, 0x2B2230);
    }
}

// Worn: a diving mask (the eyes show through) and a snorkel; bubbles rise from its top.
void p_snorkel(Ctx &x, bool draw)
{
    x.tool = x.body_to_stage(28, -132);
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    b.line(-22, -88, -33, -92, 3, 0x2F7DB8);
    b.line(22, -88, 33, -92, 3, 0x2F7DB8);
    const Path mask = Path::rect(-22, -97, 44, 21, 8);
    b.area(mask, 0xBFE6F7, vec::NONE, 0, 0.5f);
    b.stroke(mask, 2.8f, 0x2F7DB8, true);
    b.line(-16, -93, -10, -93, 1.6f, WHITE);
    b.stroke(poly({24, -76, 28, -76, 28, -128}), 4, 0xF28C38);
}

void p_snow_slope(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &s = x.scene;
    Path back, front;
    back.to(-120, -50).quad(-40, -90, 40, -60, 8).quad(100, -40, 150, -70, 8).to(150, 0).to(-120, 0);
    s.area(back, 0xEEF4FA, 0xD6E2EE, 1.6f);
    front.to(-120, -14).quad(-30, -30, 60, -12, 8).quad(110, -2, 150, -14, 8).to(150, 50).to(-120, 50);
    s.area(front, WHITE);
    static const float FIRS[3][3] = {{-54, -34, 1}, {96, -30, 1.1f}, {112, -46, 0.8f}};
    for (auto &f : FIRS) {
        scenery::fir(s, f[0], f[1], f[2], 0x3E8E5A);
        s.fill(poly({f[0], f[1] - 30 * f[2], f[0] + 6 * f[2], f[1] - 20 * f[2], f[0] - 6 * f[2], f[1] - 20 * f[2]}), WHITE);
    }
}

// Skis under the cat's feet, following her sway.
void p_skis(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen s = x.scene.move(x.v[X] - FEET_X, 0).turn(x.v[ROT] * 0.25f);
    static const float SKIS[2][2] = {{0xE5484D, -6}, {0x4BA3D9, -2}};
    for (auto &k : SKIS) {
        const float y = k[1];
        Path ski;
        ski.to(-46, y).to(44, y).quad(54, y, 56, y - 8, 4).to(52, y - 8).quad(50, y - 3, 44, y - 3, 4).to(-46, y - 3);
        s.area(ski, (uint32_t)k[0]);
    }
}

void p_beanie(Ctx &x, bool draw)
{
    if (!draw) {
        return;
    }
    const Pen &b = x.body;
    Path hat;
    hat.to(-29, -104).quad(-27, -128, 0, -129, 6).quad(27, -128, 29, -104, 6);
    b.fill(hat, 0x4BA3D9);
    for (float y : {-114.0f, -121.0f}) {
        Path stripe;
        stripe.to(-27, y).quad(0, y - 6, 27, y, 6);
        b.stroke(stripe, 2, mix(0x4BA3D9, WHITE, 0.8f));
    }
    b.rect(-31, -108, 62, 9, 4.5f, 0x2F7DB8);
    b.ell(0, -131 + sinf(x.c * 3), 6.5f, 6.5f, WHITE, 0xD6E2EE, 1.2f);
}

void pole(const Pen &s)
{
    s.line(0, 0, 0, 46, 2, 0x6B7180);
    s.stroke(Path::oval(0, 41, 4, 1.4f, 16), 1.2f, 0x3B2F4A, true);
}

void p_ski_poles(Ctx &x, bool draw)
{
    if (draw) {
        pole(x.body.move(x.L[0], x.L[1]).turn(18));
        pole(x.body.move(x.R[0], x.R[1]).turn(-18));
    }
}

// Knödel along as the cat's buddy: a fixed expression at a fixed place
// (scene frame), BUDDY_PROPS in katze.js.
struct BuddyDef {
    scenery::Mood mood;
    float x, y, k;
};
const BuddyDef BUDDIES[] = {
    {scenery::HAPPY, 54, 0, 1},    {scenery::WOW, 56, 0, 1},     {scenery::TIRED, 54, 0, 1},    {scenery::WALK, 58, 0, 1},
    {scenery::WOW, 58, -46, 0.9f}, {scenery::SLEEP, 54, -24, 1}, {scenery::POSE, 66, 0, 1},     {scenery::FLOAT, 60, -22, 1},
    {scenery::COOL, 56, 0, 1},     {scenery::HAPPY, 88, -24, 0.9f}, {scenery::SHOCK, 64, 0, 1.05f}, {scenery::HAPPY, 62, -28, 1},
    {scenery::CHEERS, 62, -28, 1}, {scenery::FULL, 62, -28, 1}, {scenery::FLAT, 54, 0, 1.05f},
    {scenery::COACH, 60, 0, 1},    {scenery::STRETCH, 72, 0, 1}, {scenery::LAUGH, 56, 0, 1},    {scenery::ROLL, 64, 0, 1},
    {scenery::YOGA, 62, -3, 1},    {scenery::FLOAT, 64, -56, 1}, {scenery::SNOW, 60, 0, 1},
};
template <int I>
void p_buddy(Ctx &x, bool draw)
{
    if (draw) {
        const BuddyDef &d = BUDDIES[I];
        scenery::buddy(x.scene.move(d.x, d.y).scale(d.k), d.mood, x.c, *x.st);
    }
}

// Where a prop is drawn: on the stage behind the cat (scene frame), on the body
// behind the torso (a backpack), worn on the body under the arms, held between
// the arms and the paws (body frame), or on the stage in front of the cat (lake
// water, a table). Within a layer, in the card's order.
enum Layer { BACK, BEHIND, WEAR, HAND, FRONT };

struct PropDef {
    const char *id;
    Layer layer;
    PropFn fn;
};
const PropDef PROPS[] = {
    {"bed", BACK, p_bed}, {"window", BACK, p_window}, {"washing-machine", BACK, p_washing_machine},
    {"drying-rack", BACK, p_drying_rack}, {"ironing-board", BACK, p_ironing_board}, {"plant", BACK, p_plant},
    {"box", HAND, p_box}, {"sponge", HAND, p_sponge}, {"towel", HAND, p_towel}, {"plate", HAND, p_plate},
    {"sock", HAND, p_sock}, {"iron", HAND, p_iron}, {"trash-bag", HAND, p_trash_bag},
    {"watering-can", HAND, p_watering_can}, {"vacuum", HAND, p_vacuum}, {"mop", HAND, p_mop},
    {"bike", BACK, p_bike}, {"bus", BACK, p_bus}, {"bus-stop", BACK, p_bus_stop}, {"train-door", BACK, p_train_door},
    {"bus-door", BACK, p_bus_door}, {"departure-board", BACK, p_departure_board},
    {"transfer-sign", BACK, p_transfer_sign}, {"station-sign", BACK, p_station_sign},
    {"suitcase", HAND, p_suitcase}, {"map", HAND, p_map}, {"welcome-sign", HAND, p_welcome_sign},
    {"umbrella", HAND, p_umbrella},
    {"clothes-rack", BACK, p_clothes_rack}, {"mirror", BACK, p_mirror}, {"shoe-shelf", BACK, p_shoe_shelf},
    {"card-terminal", BACK, p_card_terminal}, {"counter", BACK, p_counter}, {"piggy-bank", BACK, p_piggy_bank},
    {"jacket", WEAR, p_jacket}, {"scarf", WEAR, p_scarf}, {"shoe-small", HAND, p_shoe_small}, {"card", HAND, p_card},
    {"gift", HAND, p_gift}, {"phone", HAND, p_phone}, {"sweater-small", HAND, p_sweater_small}, {"toaster", HAND, p_toaster},
    {"wallet", HAND, p_wallet}, {"coin", HAND, p_coin}, {"phone-ear", HAND, p_phone_ear}, {"calendar", BACK, p_calendar},
    {"calendar-move", BACK, p_calendar_move}, {"calendar-x", BACK, p_calendar_x}, {"thermometer", WEAR, p_thermometer},
    {"big-clock", BACK, p_big_clock}, {"door", BACK, p_door}, {"number-display", BACK, p_number_display},
    {"ticket", HAND, p_ticket}, {"clipboard", HAND, p_clipboard}, {"pen", HAND, p_pen}, {"folder", HAND, p_folder},
    {"fridge", BACK, p_fridge}, {"sofa-back", BACK, p_sofa_back}, {"pillow", HAND, p_pillow}, {"red-nose", WEAR, p_red_nose},
    {"tissue", HAND, p_tissue}, {"plaster-cheek", WEAR, p_plaster_cheek},
    {"mountains", BACK, p_mountains}, {"sunset", BACK, p_sunset}, {"hills", BACK, p_hills}, {"slope", BACK, p_slope},
    {"boulder", FRONT, p_boulder}, {"big-sun", BACK, p_big_sun}, {"bench", BACK, p_bench}, {"lake-back", BACK, p_lake_back},
    {"night-window", BACK, p_night_window}, {"knoedel-sleep", BACK, p_knoedel_sleep}, {"meadow", BACK, p_meadow},
    {"summit", BACK, p_summit}, {"signpost", BACK, p_signpost}, {"hut-door", BACK, p_hut_door}, {"hut-bench", BACK, p_hut_bench},
    {"menu-board", BACK, p_menu_board}, {"car", BACK, p_car}, {"backpack-open", BACK, p_backpack_open},
    {"hiking-pole", HAND, p_hiking_pole}, {"water-bottle", HAND, p_water_bottle}, {"fuel-nozzle", HAND, p_fuel_nozzle},
    {"camera", HAND, p_camera}, {"fork", HAND, p_fork}, {"glass", HAND, p_glass}, {"knoedel-hug", HAND, p_knoedel_hug},
    {"nightcap", WEAR, p_nightcap}, {"sunglasses", WEAR, p_sunglasses}, {"backpack-straps", WEAR, p_backpack_straps},
    {"backpack", BEHIND, p_backpack}, {"knoedel-on-pack", BEHIND, p_knoedel_on_pack}, {"lake-water", FRONT, p_lake_water},
    {"table-food", FRONT, p_table_food}, {"table-drinks", FRONT, p_table_drinks}, {"table-empty", FRONT, p_table_empty},
    {"knoedel", BACK, p_buddy<0>}, {"knoedel-wow", BACK, p_buddy<1>}, {"knoedel-tired", BACK, p_buddy<2>},
    {"knoedel-walk", BACK, p_buddy<3>}, {"knoedel-top", FRONT, p_buddy<4>}, {"knoedel-bench", BACK, p_buddy<5>},
    {"knoedel-pose", BACK, p_buddy<6>}, {"knoedel-float", BACK, p_buddy<7>}, {"knoedel-cool", BACK, p_buddy<8>},
    {"knoedel-guest", BACK, p_buddy<9>}, {"knoedel-shock", BACK, p_buddy<10>}, {"knoedel-table", BACK, p_buddy<11>},
    {"knoedel-cheers", BACK, p_buddy<12>}, {"knoedel-full", BACK, p_buddy<13>}, {"knoedel-flat", BACK, p_buddy<14>},
    {"knoedel-coach", BACK, p_buddy<15>}, {"knoedel-stretch", BACK, p_buddy<16>}, {"knoedel-laugh", BACK, p_buddy<17>},
    {"knoedel-roll", BACK, p_buddy<18>}, {"knoedel-yoga", BACK, p_buddy<19>}, {"knoedel-dive", BACK, p_buddy<20>},
    {"knoedel-snow", BACK, p_buddy<21>},
    {"gym", BACK, p_gym}, {"headband", WEAR, p_headband}, {"barbell", HAND, p_barbell}, {"treadmill", BACK, p_treadmill},
    {"jump-rope", BACK, p_jump_rope}, {"punching-bag", BACK, p_punching_bag}, {"boxing-gloves", HAND, p_boxing_gloves},
    {"mat", BACK, p_mat}, {"yoga-mat", BACK, p_yoga_mat}, {"dumbbells", HAND, p_dumbbells}, {"timer", BACK, p_timer},
    {"scale", BACK, p_scale}, {"park", BACK, p_park}, {"field", BACK, p_field}, {"ball", HAND, p_ball}, {"goal", BACK, p_goal},
    {"football", BACK, p_football}, {"pullup-bar", BACK, p_pullup_bar}, {"kite", BACK, p_kite},
    {"underwater", BACK, p_underwater}, {"snorkel", WEAR, p_snorkel}, {"snow-slope", BACK, p_snow_slope}, {"skis", BACK, p_skis},
    {"beanie", WEAR, p_beanie}, {"ski-poles", HAND, p_ski_poles},
};
constexpr int PROP_COUNT = sizeof(PROPS) / sizeof(PROPS[0]);

void draw_behind(Ctx &x)
{
    for (int i = 0; i < x.anim->n_prop; i++) {
        if (PROPS[x.anim->prop[i]].layer == BEHIND) {
            PROPS[x.anim->prop[i]].fn(x, true);
        }
    }
}

// ---- effects (stage frame) ---------------------------------------------------------

using FxFn = void (*)(const Ctx &x);

void star(const Ctx &x, float px, float py, float scale, float a, uint32_t color)
{
    if (a < 0.05f || !x.inside(px, py, 6 * scale)) {
        return;
    }
    const uint32_t col = x.fade(color, a);
    const float r = 6 * scale, core = 1.9f * scale, tip = 0.3f / x.stage.k;
    x.stage.wedge(px, py, px + r, py, core, tip, col);
    x.stage.wedge(px, py, px - r, py, core, tip, col);
    x.stage.wedge(px, py, px, py + r, core, tip, col);
    x.stage.wedge(px, py, px, py - r, core, tip, col);
}

// Three wisps rising: steam, or a smell.
void wisps(const Ctx &x, const Anchor &from, float dx0, float dx, float dy0, float rise, float speed, float width,
           uint32_t color)
{
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * speed + i / 3.0f), a = x.w * sinf(PI_F * k);
        const float px = from.x + dx0 + i * dx, py = from.y + dy0 - k * rise;
        if (a < 0.08f || !x.inside(px, py - 8, 9)) {
            continue;
        }
        Path wisp;
        wisp.to(0, 0).quad(-3, -4, 0, -8, 4).quad(3, -12, 0, -16, 4);
        x.stage.move(px, py).stroke(wisp, width, x.fade(color, a));
    }
}

void fx_sparkles(const Ctx &x)
{
    const Anchor o = x.plate.set ? x.plate : x.tool.set ? x.tool : Anchor{x.v[X] + 40, x.body_y - 10, true};
    static const float POS[4][2] = {{-16, -16}, {16, -10}, {-12, 14}, {18, 16}};
    for (int i = 0; i < 4; i++) {
        const float tw = fabsf(sinf(x.c * 3 + i * 1.3f));
        star(x, o.x + POS[i][0], o.y + POS[i][1], 0.35f + tw * 0.65f, x.w * tw, STAR);
    }
}

void fx_bubbles(const Ctx &x)
{
    const Anchor o = x.tool.set ? x.tool : x.drum.set ? x.drum : Anchor{x.v[X] + 30, x.body_y, true};
    for (int i = 0; i < 6; i++) {
        const float k = fract(x.c * 0.45f + i / 6.0f), r = 2.5f + (i % 3), a = x.w * sinf(PI_F * k) * 0.95f;
        const float px = o.x + sinf(k * 6 + i) * 6 + ((i % 2) ? 6 : -6), py = o.y - k * 34;
        if (a < 0.05f || !x.inside(px, py, r + 1)) {
            continue;
        }
        x.stage.ell(px, py, r + 0.5f, r + 0.5f, 0x9FC4E8, a);
        x.stage.ell(px, py, r - 0.5f, r - 0.5f, WHITE, a);
    }
}

void fx_dust(const Ctx &x)
{
    const Anchor o = x.vac.set ? x.vac : Anchor{x.v[X] + 50, x.floor_y - 4, true};
    for (int i = 0; i < 5; i++) {
        const float k = fract(x.c * 0.8f + i / 5.0f), r = 3.2f * (1 - k * 0.6f);
        const float px = o.x + 2 + (1 - k) * 18, py = o.y - 2 - sinf(k * 9 + i) * 4 * (1 - k);
        if (x.inside(px, py, r)) {
            x.stage.ell(px, py, r, r, 0xE9A766, x.w * (k < 0.85f ? 1 : (1 - k) / 0.15f));
        }
    }
}

// A wet, shining floor behind the cat.
void fx_shine(const Ctx &x)
{
    const Anchor o = x.mop.set ? x.mop : Anchor{x.v[X] + 40, x.floor_y, true};
    x.stage.ell(o.x, x.floor_y + 1, 26, 3.4f, 0x9FD3EE, x.w * 0.7f);
    for (int i = 0; i < 3; i++) {
        const float tw = fabsf(sinf(x.c * 2.4f + i * 2));
        star(x, o.x - 16 + i * 16, x.floor_y - 2, 0.3f + tw * 0.4f, x.w * tw, WHITE);
    }
}

void fx_steam(const Ctx &x)
{
    wisps(x, x.steam.set ? x.steam : x.iron.set ? x.iron : Anchor{x.v[X] + 40, x.body_y, true}, -6, 6, 0, 14, 0.6f, 2, 0xC9D3DE);
}

void fx_stink(const Ctx &x)
{
    wisps(x, x.bag.set ? x.bag : Anchor{x.v[X] + 40, x.body_y, true}, -10, 10, 6, 18, 0.5f, 2.2f, 0x7DB35A);
}

void fx_drops(const Ctx &x)
{
    const Anchor o = x.spout.set ? x.spout : Anchor{x.v[X] + 50, x.body_y, true};
    const float on = x.pour < 0 ? 1 : x.pour;
    for (int i = 0; i < 4; i++) {
        const float k = fract(x.c * 1.4f + i / 4.0f), a = x.w * on * (1 - k * 0.6f);
        const float px = o.x + 2 + k * 3, py = o.y + 3 + k * 22;
        if (a > 0.05f && x.inside(px, py, 4)) {
            x.stage.wedge(px, py - 3.2f, px, py + 1.8f, 0.3f, 2.1f, x.fade(0x6FB8E6, a));
        }
    }
}

// The sun, behind the cat at the upper left. A little further in than in the
// preview, so that its rays stay on the disc.
void fx_sun(const Ctx &x)
{
    const Pen sun = x.stage.move(FEET_X - 50, FEET_Y - 112);
    const uint32_t col = x.fade(STAR, x.w);
    sun.ell(0, 0, 9, 9, STAR, x.w);
    for (int i = 0; i < 8; i++) {
        const float a = (i * 45 + x.c * 20) * DEG;
        sun.line(sinf(a) * 12.5f, -cosf(a) * 12.5f, sinf(a) * 17, -cosf(a) * 17, 2.4f, col);
    }
}

// ---- a2-unterwegs-verben ----

// A teardrop at (px, py), turned by `degrees`.
void drop(const Ctx &x, float px, float py, float degrees, float a)
{
    if (a < 0.05f || !x.inside(px, py, 5)) {
        return;
    }
    const float r = degrees * DEG, sx = sinf(r), cy = cosf(r);
    x.stage.wedge(px + sx * 5, py - cy * 5, px - sx * 2.5f, py + cy * 2.5f, 0.3f, 3, x.fade(0x6FB8E6, a));
}

void fx_speed_lines(const Ctx &x)
{
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 2 + i / 3.0f), px = x.v[X] - 46 - k * 22, py = x.body_y - 34 + i * 20;
        if (x.inside(px - 8, py, 10)) {
            x.stage.line(px, py, px - 16, py, 3, x.fade(WHITE, x.w * (1 - k) * 0.95f));
        }
    }
}

void fx_sweat(const Ctx &x)
{
    for (int i = 0; i < 2; i++) {
        const float k = fract(x.c * 0.9f + i * 0.5f), side = i ? 1 : -1;
        drop(x, x.v[X] + side * (34 + k * 8), x.head_y + 26 + k * 10, side * 30, x.w * (1 - k));
    }
}

void fx_sigh_puffs(const Ctx &x)
{
    const float k = fract(x.c * 0.45f), a = x.w * sinf(PI_F * k) * 0.9f;
    for (int i = 0; i < 3; i++) {
        const float r = 2.5f + i * 1.3f + k * 2, px = x.v[X] + 10 + k * 26 + i * 7, py = x.mouth_y - k * 10 - i * 3;
        if (a > 0.05f && x.inside(px, py, r)) {
            x.stage.ell(px, py, r, r, WHITE, a);
        }
    }
}

// Two question marks pop up beside the head.
void fx_question(const Ctx &x)
{
    for (int i = 0; i < 2; i++) {
        const float k = fract(x.c * 0.7f + i * 0.5f), a = x.w * (k < 0.85f ? 1 : (1 - k) / 0.15f);
        if (k < 0.08f || a < 0.3f) {
            continue;
        }
        x.stage.text(i ? x.st->font_small : x.st->font_big, "?", x.v[X] + (i ? -32 : 34), x.head_y + (i ? 0 : -10),
                     middle_center, x.fade(0x7A4FD6, a));
    }
}

void fx_hearts(const Ctx &x)
{
    static const float HX[3] = {-34, 36, 26};
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.5f + i / 3.0f), a = x.w * sinf(PI_F * k);
        const float px = x.v[X] + HX[i] + sinf(k * 7) * 3, py = x.head_y + 40 - k * 34;
        if (a > 0.05f && x.inside(px, py, 8)) {
            x.stage.move(px, py).scale(0.6f + k * 0.5f).fill(heart_path(), x.fade(0xF06C8B, a));
        }
    }
}

// Rain over the whole stage, behind the cat.
void fx_rain(const Ctx &x)
{
    const uint32_t col = x.fade(0x7FA6C4, x.w * 0.8f);
    for (int i = 0; i < 12; i++) {
        const float k = fract(x.c * 1.1f + i * 0.37f), px = x.v[X] - 70 + (i * 53) % 190, py = x.head_y - 40 + k * 190;
        if (x.inside(px, py, 6)) {
            x.stage.line(px, py, px - 3, py + 9, 1.8f, col);
        }
    }
}

// ---- a2-einkaufen-verben and a2-termine-verben ----

// Part of a circle round (cx, cy), from angle a0 to a1 (degrees, clockwise on screen).
void arc(const Pen &p, float cx, float cy, float r, float a0, float a1, float w, uint32_t col)
{
    Path q;
    const int steps = 10;
    for (int i = 0; i <= steps; i++) {
        const float a = (a0 + (a1 - a0) * i / steps) * DEG;
        q.to(cx + cosf(a) * r, cy + sinf(a) * r);
    }
    p.stroke(q, w, col);
}

// A pop-up badge: grows in with a little overshoot, fades out at the end.
float pop_scale(float k) { return k < 0.2f ? fmaxf(0.01f, 1 + 2.7f * powf(k / 0.2f - 1, 3) + 1.7f * powf(k / 0.2f - 1, 2)) : 1; }

void fx_contactless(const Ctx &x)
{
    const Anchor o = x.card.set ? x.card : Anchor{x.v[X] + 50, x.body_y, true};
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 1.2f - i * 0.2f), a = x.w * fmaxf(0, sinf(PI_F * k)), r = 6 + i * 5;
        if (a > 0.05f && x.inside(o.x + r, o.y, 3)) {
            arc(x.stage, o.x, o.y, r, -90, 90, 2, x.fade(0x7C6BD6, a));
        }
    }
}

// A parcel flies off from the phone.
void fx_parcel(const Ctx &x)
{
    const Anchor o = x.phone.set ? x.phone : Anchor{x.v[X], x.body_y - 30, true};
    const float k = fract(x.c * 0.5f), a = x.w * (k < 0.8f ? 1 : (1 - k) / 0.2f);
    const float px = o.x + 22 + k * 40, py = o.y - 6 - sinf(PI_F * k) * 24 - k * 8;
    if (a < 0.05f || !x.inside(px, py, 8)) {
        return;
    }
    const Pen box = x.stage.move(px, py).scale(0.6f + k * 0.5f);
    box.rect(-7, -6, 14, 12, 1.5f, x.fade(0xD9A86C, a), x.fade(0xB0803F, a), 1.2f);
    box.line(-7, -1, 7, -1, 1.2f, x.fade(0xB0803F, a));
    box.line(0, -6, 0, 6, 1.2f, x.fade(0xB0803F, a));
}

// Two arrows chasing each other round: exchange.
void fx_swap(const Ctx &x)
{
    const Pen p = x.stage.move(x.v[X] + 40, x.head_y + 16).turn(fmodf(x.c * 90, 360));
    const uint32_t col = x.fade(0x1F8FA8, x.w);
    arc(p, 0, 0, 11, 202, 326, 2.6f, col);
    p.line(9, -6, 8, -11, 2.6f, col);
    p.line(9, -6, 4, -5, 2.6f, col);
    arc(p, 0, 0, 11, 22, 146, 2.6f, col);
    p.line(-9, 6, -8, 11, 2.6f, col);
    p.line(-9, 6, -4, 5, 2.6f, col);
}

// Coins dropping out of the wallet.
void fx_coins(const Ctx &x)
{
    const Anchor o = x.wallet.set ? x.wallet : Anchor{x.v[X] + 34, x.body_y - 20, true};
    for (int i = 0; i < 4; i++) {
        const float k = fract(x.c * 0.9f + i / 4.0f), a = x.w * (1 - k);
        const float px = o.x + (i - 1.5f) * 5 + k * 6, py = o.y + 6 + k * k * 50;
        if (a > 0.05f && x.inside(px, py, 4)) {
            x.stage.ell(px, py, 3.4f, 3.4f, x.fade(0xF2C14E, a), x.fade(0xC99227, a), 1);
        }
    }
}

// The phone rings at the ear.
void fx_ring(const Ctx &x)
{
    const Anchor o = x.ear.set ? x.ear : Anchor{x.v[X] + 34, x.head_y + 40, true};
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 1.5f + i / 3.0f), r = 6 + k * 10, a = x.w * (1 - k);
        if (a > 0.05f && x.inside(o.x + r, o.y, 3)) {
            arc(x.stage, o.x, o.y, r, -45, 45, 2, x.fade(0x2E9A7A, a));
        }
    }
}

// A badge next to the phone: call back, or confirmed.
void badge(const Ctx &x, float speed, bool check)
{
    const Anchor o = x.phone.set ? x.phone : Anchor{x.v[X] + 30, x.body_y - 40, true};
    const float k = fract(x.c * speed), a = x.w * (k < 0.85f ? 1 : (1 - k) / 0.15f), px = o.x + (check ? 36 : 34), py = o.y - 16;
    if (a < 0.05f || !x.inside(px, py, 12)) {
        return;
    }
    const Pen p = x.stage.move(px, py).scale(pop_scale(k));
    if (check) {
        p.ell(0, 0, 12, 12, x.fade(0x3E8E2F, a));
        Path tick;
        tick.to(-6, 0).to(-2, 5).to(7, -5);
        p.stroke(tick, 3, x.fade(WHITE, a));
    } else {
        p.ell(0, 0, 11, 11, x.fade(WHITE, a), x.fade(0x2F7DB8, a), 1.6f);
        arc(p, 0, 0, 6, -31, -321, 2.2f, x.fade(0x2F7DB8, a));
        p.line(5, -3, 6, -7, 2.2f, x.fade(0x2F7DB8, a));
        p.line(5, -3, 1, -4, 2.2f, x.fade(0x2F7DB8, a));
    }
}
void fx_callback(const Ctx &x) { badge(x, 0.8f, false); }
void fx_checkmark(const Ctx &x) { badge(x, 0.6f, true); }

// Short strokes at the paw while it knocks.
void fx_knock_lines(const Ctx &x)
{
    const Anchor o = x.paw_r.set ? x.paw_r : Anchor{x.v[X] + 40, x.body_y - 20, true};
    if (sinf(x.c * 1.6f) <= -0.2f || sinf(x.c * 11) <= 0.3f) {
        return;
    }
    const uint32_t col = x.fade(0x7A4A2E, x.w);
    static const float D[3][2] = {{-6, -10}, {0, -12}, {6, -10}};
    for (auto &d : D) {
        x.stage.line(o.x + 8 + d[0], o.y + d[1], o.x + 8 + d[0] * 1.5f, o.y + d[1] - 5, 2, col);
    }
}

// A signature being written on the clipboard.
void fx_signature(const Ctx &x)
{
    const Anchor o = x.clip.set ? x.clip : Anchor{x.v[X] + 10, x.body_y, true};
    Path sig;
    sig.to(-8, 0).quad(-6, -6, -4, 0, 4).quad(-2, 6, 0, 0, 4).quad(2, -5, 3, 1, 4).quad(4, 4, 8, -2, 4);
    float total = 0;
    for (int i = 0; i + 1 < sig.n; i++) {
        total += hypotf(sig.p[i + 1][0] - sig.p[i][0], sig.p[i + 1][1] - sig.p[i][1]);
    }
    const float shown = total * clampf(fract(x.c * 0.45f) * 1.4f);
    const Pen p = x.stage.move(o.x, o.y + 4);
    const uint32_t col = x.fade(0x2F5DA8, x.w);
    float done = 0;
    for (int i = 0; i + 1 < sig.n && done < shown; i++) {
        const float len = hypotf(sig.p[i + 1][0] - sig.p[i][0], sig.p[i + 1][1] - sig.p[i][1]);
        const float t = fminf(1, (shown - done) / len);
        p.line(sig.p[i][0], sig.p[i][1], sig.p[i][0] + (sig.p[i + 1][0] - sig.p[i][0]) * t,
               sig.p[i][1] + (sig.p[i + 1][1] - sig.p[i][1]) * t, 1.6f, col);
        done += len;
    }
}

// ---- a2-gesundheit-nomen ----

// Puffs from the mouth with each cough (motion "cough").
void fx_cough_puffs(const Ctx &x)
{
    const float u = fmodf(x.c, 1.8f) / 1.8f;
    for (int i = 0; i < 3; i++) {
        const float k = clampf((u - 0.08f - i * 0.04f) / 0.4f), a = x.w * (k > 0 && k < 1 ? 1 - k : 0);
        const float px = x.v[X] + 8 + k * 26 + i * 4, py = x.mouth_y - 2 - k * 6 + i * 3, r = 2 + k * 4;
        if (a > 0.05f && x.inside(px, py, r)) {
            x.stage.ell(px, py, r, r, 0xD6DCE6, a);
        }
    }
}

// Zigzags of pain beside the back.
void fx_pain(const Ctx &x)
{
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 1.2f + i / 3.0f), a = x.w * sinf(PI_F * k);
        const float px = x.v[X] + 34 + k * 6, py = x.body_y - 14 + i * 12;
        if (a < 0.05f || !x.inside(px + 6, py - 2, 8)) {
            continue;
        }
        Path zig;
        zig.to(0, 0).to(4, -4).to(8, 0).to(12, -4);
        x.stage.move(px, py).stroke(zig, 2.2f, x.fade(0xE5484D, a));
    }
}

// ---- a2-wandern-verben and a2-huette-phrasen ----

// The camera's flash.
void fx_flash(const Ctx &x)
{
    const Anchor o = x.flash.set ? x.flash : Anchor{x.v[X] + 40, x.head_y + 40, true};
    const float k = fmodf(x.c, 1.6f) / 1.6f, on = k < 0.16f ? sinf(PI_F * k / 0.16f) : 0;
    if (on * x.w < 0.05f) {
        return;
    }
    if (x.inside(o.x, o.y, 14)) {
        x.stage.ell(o.x, o.y, 14, 14, WHITE, 0.75f * on * x.w);
    }
    scenery::star(x.stage.move(o.x, o.y).scale(1 + on * 1.4f), x.fade(0xFFF3B0, on * x.w));
}

// Sparkles where the glasses meet (Knödel's glass is at 138, 108).
void fx_clink(const Ctx &x)
{
    const Anchor a = x.glass.set ? x.glass : Anchor{x.v[X] + 30, x.head_y + 40, true};
    const float ox = (a.x + 138) / 2 + 4, oy = (a.y + 108) / 2 - 8, on = fmaxf(0, sinf(x.c * 2.4f));
    if (on * x.w < 0.05f) {
        return;
    }
    static const float POS[3][2] = {{0, -6}, {-8, 2}, {8, 3}};
    for (auto &q : POS) {
        scenery::star(x.stage.move(ox + q[0] * (1 + on * 0.5f), oy + q[1] * (1 + on * 0.5f)).scale(0.3f + on * 0.5f),
                      x.fade(STAR, on * x.w));
    }
}

// ---- a2-sport-draussen ----

// Gusts of wind drifting across.
void fx_breeze(const Ctx &x)
{
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.35f + i / 3.0f), a = 0.85f * x.w * sinf(PI_F * k);
        const float px = x.v[X] - 90 + k * 170, py = x.head_y + 10 + i * 28;
        if (a < 0.05f || !x.inside(px + 20, py, 22)) {
            continue;
        }
        Path gust;
        gust.to(0, 0).quad(10, -5, 20, 0, 4).quad(30, 5, 40, 0, 4);
        x.stage.move(px, py).stroke(gust, 2.4f, x.fade(WHITE, a));
    }
}

// Snow falling over the whole disc.
void fx_snow(const Ctx &x)
{
    for (int i = 0; i < 16; i++) {
        const float py = fmodf(x.c * 16 + i * 31, 220) - 30, px = fmodf(i * 47.0f, 230) - 15 + sinf(x.c + i) * 5;
        const float r = 1.4f + (i % 3) * 0.6f;
        if (x.inside(px, py, r)) {
            x.stage.ell(px, py, r, r, WHITE, 0xC9D8E8, 0.6f);
        }
    }
}

struct FxDef {
    const char *id;
    bool back;  // drawn behind the cat
    FxFn fn;
};
const FxDef EFFECTS[] = {
    {"sparkles", false, fx_sparkles}, {"bubbles", false, fx_bubbles}, {"dust", false, fx_dust}, {"shine", true, fx_shine},
    {"steam", false, fx_steam}, {"stink", false, fx_stink}, {"drops", false, fx_drops}, {"sun", true, fx_sun},
    {"speed-lines", false, fx_speed_lines}, {"sweat", false, fx_sweat}, {"sigh-puffs", false, fx_sigh_puffs},
    {"question", false, fx_question}, {"hearts", false, fx_hearts}, {"rain", true, fx_rain},
    {"contactless", false, fx_contactless}, {"parcel", false, fx_parcel}, {"swap", false, fx_swap},
    {"coins", false, fx_coins}, {"ring", false, fx_ring}, {"callback", false, fx_callback},
    {"checkmark", false, fx_checkmark}, {"knock-lines", false, fx_knock_lines}, {"signature", false, fx_signature},
    {"cough-puffs", false, fx_cough_puffs}, {"pain", false, fx_pain}, {"flash", false, fx_flash}, {"clink", false, fx_clink},
    {"breeze", false, fx_breeze}, {"snow", false, fx_snow},
};
constexpr int FX_COUNT = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

// Neutral, ease into the target pose, hold, ease back out.
void pose_at(const Anim &anim, float t, float *v)
{
    float u;
    if (t < 0.5f) {
        u = 0;
    } else if (t < 1.1f) {
        u = ease((t - 0.5f) / 0.6f);
    } else if (t < LOOP_SECONDS - 0.6f) {
        u = 1;
    } else {
        u = 1.0f - ease(clampf((t - (LOOP_SECONDS - 0.6f)) / 0.6f));
    }
    memcpy(v, NEUTRAL, sizeof(NEUTRAL));
    for (int i = 0; i < POSE_PARAMS; i++) {
        v[JUMP + i] += (anim.pose[i] - NEUTRAL[JUMP + i]) * u;
    }
}

int k_count(Kind kind) { return kind == POSE ? POSE_PARAMS : kind == MOTION ? MOTION_COUNT : kind == FX ? FX_COUNT : kind == PROP ? PROP_COUNT : 0; }
const char *k_name(Kind kind, int i)
{
    return kind == POSE ? POSE_NAMES[i] : kind == MOTION ? MOTIONS[i].id : kind == FX ? EFFECTS[i].id : PROPS[i].id;
}
float k_pose_neutral(int index) { return NEUTRAL[JUMP + index]; }

void k_draw(G *g, const Stage &st, const Anim &anim, float t, float c)
{
    const float u = st.r / 100.0f;  // pixels per unit of the 200 x 200 disc
    const float s = u * CAT_SCALE;

    float v[P_COUNT];
    pose_at(anim, t, v);
    const float w = clampf((t - 0.9f) / 0.4f) * clampf((LOOP_SECONDS - 0.35f - t) / 0.45f);
    for (int i = 0; i < anim.n_motion; i++) {
        MOTIONS[anim.motion[i]].fn(v, c, w);
    }

    Ctx x = {};
    x.st = &st;
    x.anim = &anim;
    x.v = v;
    x.c = c;
    x.w = w;
    x.stage = Pen::make(g, st.cx + (FEET_X - 100) * u - FEET_X * s, st.cy + (FEET_Y - 100) * u - FEET_Y * s, s);
    x.scene = x.stage.move(FEET_X, FEET_Y);
    x.root = x.stage.move(v[X], v[Y] - v[JUMP]).scale(v[SX], v[SY]);
    x.body = x.root.turn(v[ROT], 0, -10);
    x.head_y = v[Y] - v[JUMP] - 134 * v[SY];
    x.body_y = v[Y] - v[JUMP] - 50 * v[SY];
    x.floor_y = v[Y];
    x.mouth_y = v[Y] - v[JUMP] - 73 * v[SY];
    x.pour = -1;
    float elbow_l[2], elbow_r[2];
    reach(-1, v[HLX], v[HLY], elbow_l, x.L);
    reach(1, v[HRX], v[HRY], elbow_r, x.R);

    for (int i = 0; i < anim.n_prop; i++) {
        PROPS[anim.prop[i]].fn(x, false);
    }
    x.paw_l = x.body_to_stage(x.L[0], x.L[1]);
    x.paw_r = x.body_to_stage(x.R[0], x.R[1]);
    for (int i = 0; i < anim.n_prop; i++) {
        if (PROPS[anim.prop[i]].layer == BACK) {
            PROPS[anim.prop[i]].fn(x, true);
        }
    }
    x.stage.ell(v[X], FEET_Y + 1, 32 * v[SX] - v[JUMP] * 0.4f, 4.5f, 0x000000, 0.08f);
    for (int i = 0; i < anim.n_fx && w >= 0.03f; i++) {
        if (EFFECTS[anim.fx[i]].back) {
            EFFECTS[anim.fx[i]].fn(x);
        }
    }

    draw_legs(x);
    draw_body(x);
    for (int i = 0; i < anim.n_prop; i++) {
        if (PROPS[anim.prop[i]].layer == WEAR) {
            PROPS[anim.prop[i]].fn(x, true);
        }
    }
    limb(x.body, -SHOULDER, SHOULDER_Y, elbow_l[0], elbow_l[1], 6, 5.4f);
    limb(x.body, elbow_l[0], elbow_l[1], x.L[0], x.L[1], 5.4f, 5);
    limb(x.body, SHOULDER, SHOULDER_Y, elbow_r[0], elbow_r[1], 6, 5.4f);
    limb(x.body, elbow_r[0], elbow_r[1], x.R[0], x.R[1], 5.4f, 5);
    for (int i = 0; i < anim.n_prop; i++) {
        if (PROPS[anim.prop[i]].layer == HAND) {
            PROPS[anim.prop[i]].fn(x, true);
        }
    }
    x.body.ell(x.L[0], x.L[1], 6, 5.4f, C_BASE, C_EDGE, EDGE_W);
    x.body.ell(x.R[0], x.R[1], 6, 5.4f, C_BASE, C_EDGE, EDGE_W);
    for (int i = 0; i < anim.n_prop; i++) {
        if (PROPS[anim.prop[i]].layer == FRONT) {
            PROPS[anim.prop[i]].fn(x, true);
        }
    }

    for (int i = 0; i < anim.n_fx && w >= 0.03f; i++) {
        if (!EFFECTS[anim.fx[i]].back) {
            EFFECTS[anim.fx[i]].fn(x);
        }
    }
}

}  // namespace

const Impl IMPL = {"katze", k_count, k_name, k_pose_neutral, {{30, 40, 18}, {176, 150, 30}, {168, 26, 12}}, true, k_draw};

}  // namespace katze
