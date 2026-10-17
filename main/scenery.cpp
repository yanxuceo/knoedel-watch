#include "scenery.h"

#include <initializer_list>

using namespace gfxu;
using vec::Path;
using vec::Pen;

namespace scenery {

namespace {

constexpr float PI_F = 3.14159265f;
constexpr uint32_t WHITE = 0xFFFFFF;

// Knödel's colours (knoedel.js, KNM in katze.js).
constexpr uint32_t K_BASE = 0xEECB8C, K_EDGE = 0xCFA563, K_SPOT = 0xC99A55, K_HERB = 0x5E9E45, K_FOOT = 0xD6AC6A;
constexpr uint32_t K_ARM = 0xE2BB7B, K_INK = 0x3A2420, K_MOUTH = 0x5B2D2A, K_BLUSH = 0xF59A9A;

Path poly(std::initializer_list<float> xy)
{
    Path q;
    for (auto i = xy.begin(); i != xy.end(); i += 2) {
        q.to(i[0], i[1]);
    }
    return q;
}

float fract(float v) { return v - floorf(v); }

// Arms of each mood: left and right angle (0 hangs down, 180 points up).
const float ARMS[][2] = {{20, 130}, {150, 150}, {6, 6},   {165, 165}, {12, 12}, {80, 80}, {20, 20},
                         {25, 150}, {140, 140}, {15, 15}, {95, 95},   {15, 50}, {70, 70},
                         {20, 110}, {20, 172},  {40, 40}, {60, 60},   {174, 174}, {20, 130}};

}  // namespace

const Palette DAY = {0xEEE7DD, 0xC9BBA8, 0xDDD2C2, 0xFFFFFF, 0xB2D896, 0x93C474};
const Palette DUSK = {0xF8C9B8, 0xE29A86, 0xF0B2A0, 0xFFF1EA, 0xB9B2DA, 0x9C93C9};

const lgfx::IFont *font_for(const character::Stage &st, float px)
{
    return px < 17 ? st.font_tiny : px < 36 ? st.font_small : st.font_big;
}

void star(const Pen &p, uint32_t col)
{
    p.fill(poly({0, -6, 1.6f, -1.6f, 6, 0, 1.6f, 1.6f, 0, 6, -1.6f, 1.6f, -6, 0, -1.6f, -1.6f}), col);
}

void buddy(const Pen &o, Mood mood, float c, const character::Stage &st)
{
    const auto fade = [&](uint32_t col, float a) { return mix(st.bg, col, a); };
    float jump = 0, sx = 1, sy = 1, rot = 0, x = 0, al = ARMS[mood][0], ar = ARMS[mood][1], lift_l = 0, lift_r = 0;
    switch (mood) {
    case FLAT:
        sx = 1.36f + sinf(c * 2) * 0.03f;
        sy = 0.58f;
        break;
    case HAPPY:
        jump = fmaxf(0, sinf(c * 2.4f)) * 2;
        ar += sinf(c * 8) * 22;
        break;
    case WOW: {
        const float s = fabsf(sinf(c * 4.5f));
        jump = s * 7;
        sy = 1 - 0.07f * (1 - s);
        sx = 1 + 0.05f * (1 - s);
        al += sinf(c * 9) * 12;
        ar += sinf(c * 9 + 1) * 12;
        break;
    }
    case TIRED: {
        const float k = sinf(c * 1.6f);
        sy = 0.95f + k * 0.025f;
        sx = 1.04f - k * 0.02f;
        rot = -5;
        break;
    }
    case SHOCK:
        x = sinf(c * 38) * 0.9f;
        al += sinf(c * 20) * 6;
        ar += sinf(c * 20 + 1) * 6;
        break;
    case SLEEP: {
        const float k = sinf(c * 1.8f);
        sy = 1 + k * 0.03f;
        sx = 1 - k * 0.015f;
        break;
    }
    case COOL:
        rot = sinf(c * 1.4f) * 4;
        al += sinf(c * 1.4f) * 5;
        break;
    case CHEERS:
        jump = fmaxf(0, sinf(c * 2.4f)) * 3;
        break;
    case POSE:
        rot = sinf(c * 2) * 5;
        jump = fabsf(sinf(c * 2)) * 1.5f;
        break;
    case WALK: {
        const float s = sinf(c * 6);
        jump = fabsf(s) * 2.5f;
        al += s * 24;
        ar -= s * 24;
        lift_l = fmaxf(0, s) * 2.6f;
        lift_r = fmaxf(0, -s) * 2.6f;
        break;
    }
    case FLOAT:
        jump = sinf(c * 2) * 2.2f;
        rot = sinf(c * 1.3f) * 6;
        al += sinf(c * 4) * 14;
        ar += sinf(c * 4 + 2) * 14;
        break;
    case FULL:
    case HUG:
        rot = sinf(c * 1.6f) * 3;
        break;
    case COACH:
        ar += sinf(c * 6) * 18;
        jump = fmaxf(0, sinf(c * 3)) * 1.5f;
        break;
    case STRETCH:
        rot = -14 + sinf(c * 1.4f) * 4;
        break;
    case LAUGH:
        rot = sinf(c * 14) * 5;
        jump = fabsf(sinf(c * 7)) * 2;
        break;
    case ROLL:
        rot = fmodf(c * 300, 360);
        jump = fabsf(sinf(c * 5.2f)) * 2;
        break;
    case YOGA:
        rot = sinf(c * 1.6f) * 7;
        x = sinf(c * 1.6f);
        lift_r = 7.2f;  // standing on one foot
        break;
    case SNOW:
        jump = fmaxf(0, sinf(c * 2.4f)) * 2.5f;
        ar += sinf(c * 8) * 22;
        break;
    }

    const Pen b = o.move(x, -jump).turn(rot, 0, -10).scale(sx, sy);
    b.ell(-6, -1.8f - lift_l, 5, 2.6f, K_FOOT);
    b.ell(6, -1.8f - lift_r, 5, 2.6f, K_FOOT);
    b.fill(Path::oval(0, -16, 15, 15, 28), K_BASE, K_EDGE, 1.2f);
    static const float SPOTS[5][4] = {{-9, -23, 1.4f, 1}, {8, -9, 1.2f, 0.9f}, {-4, -6, 1, 0.8f}, {10, -21, 1.1f, 0.8f}, {-11, -12, 1, 0.7f}};
    for (auto &s : SPOTS) {
        b.ell(s[0], s[1], s[2], s[3], mix(K_BASE, K_SPOT, 0.55f));
    }
    static const float HERBS[5][2] = {{-6, -27}, {5, -26}, {11, -14}, {-11, -7}, {2, -4}};
    for (int i = 0; i < 5; i++) {
        const float a = i * 47 * PI_F / 180;
        b.line(HERBS[i][0], HERBS[i][1], HERBS[i][0] + 1.5f * cosf(a) + 0.7f * sinf(a),
               HERBS[i][1] + 1.5f * sinf(a) - 0.7f * cosf(a), 1.2f, K_HERB);
    }
    b.turn(-32, -6, -24).oval(-6, -24, 4.2f, 2.1f, mix(K_BASE, WHITE, 0.4f));
    for (int side = -1; side <= 1; side += 2) {
        const Pen arm = b.move(side * 14, -16).turn(side < 0 ? al : -ar);
        arm.line(0, 0, 0, 7, 3.2f, K_ARM);
        arm.ell(0, 8, 2.4f, 2.4f, K_ARM, K_EDGE, 0.6f);
    }

    const Pen f = b.move(0, -17);
    const auto curve = [&](float x0, float y0, float cx, float cy, float x1, float y1, float w, uint32_t col) {
        Path q;
        q.to(x0, y0).quad(cx, cy, x1, y1, 4);
        f.stroke(q, w, col);
    };
    const auto eye_open = [&](float ex) {
        f.ell(ex, -1, 1.6f, 2.1f, K_INK);
        f.ell(ex + 0.5f, -1.8f, 0.55f, 0.55f, WHITE);
    };
    const auto eye_happy = [&](float ex) { curve(ex - 2, 0, ex, -2.8f, ex + 2, 0, 1.3f, K_INK); };
    switch (mood) {
    case WOW:
    case CHEERS:
    case FULL:
    case HUG:
    case FLOAT:
    case LAUGH:
    case ROLL:
    case SNOW:
        eye_happy(-5);
        eye_happy(5);
        break;
    case SLEEP:
    case STRETCH:
    case YOGA:
        for (float ex : {-5.0f, 5.0f}) {
            curve(ex - 2, -1, ex, 1.2f, ex + 2, -1, 1.3f, K_INK);
        }
        break;
    case TIRED:
        for (float ex : {-5.0f, 5.0f}) {
            curve(ex - 2.1f, -0.3f, ex, 0.6f, ex + 2.1f, -0.3f, 1.3f, K_INK);
        }
        break;
    case FLAT:
        f.stroke(poly({-7, -2.2f, -3.6f, -0.6f, -7, 1}), 1.3f, K_INK);
        f.stroke(poly({7, -2.2f, 3.6f, -0.6f, 7, 1}), 1.3f, K_INK);
        break;
    case SHOCK:
        for (float ex : {-5.0f, 5.0f}) {
            f.ell(ex, -1.5f, 3.2f, 3.2f, WHITE, K_INK, 0.9f);
            f.ell(ex, -1.3f, 1.1f, 1.1f, K_INK);
        }
        break;
    case POSE:
        eye_happy(-5);
        eye_open(5);
        break;
    case COOL:
        for (int side = -1; side <= 1; side += 2) {
            Path lens;
            lens.to(side * 9.5f, -3.5f).to(side * 1.6f, -3.5f).to(side * 1.6f, -1.4f).to(side * 3.2f, 0.8f);
            lens.quad(side * 6, 3, side * 8.6f, 0.6f, 4);
            f.fill(lens, 0x2B2230);
        }
        f.rect(-1.6f, -3.5f, 3.2f, 2.1f, 0, 0x2B2230);
        f.line(-7.4f, -2.2f, -5, -2.2f, 0.9f, mix(0x2B2230, WHITE, 0.6f));
        f.line(2.6f, -2.2f, 5, -2.2f, 0.9f, mix(0x2B2230, WHITE, 0.6f));
        break;
    default:
        eye_open(-5);
        eye_open(5);
    }
    switch (mood) {
    case SHOCK:
        f.ell(0, 6.4f, 2.5f, 3.4f, K_MOUTH);
        break;
    case COACH:  // a whistle
        f.area(poly({-2, 3.6f, 5.5f, 3.2f, 7.6f, 3.6f, 8.5f, 6, 7.6f, 8.2f, 5.5f, 8.6f, -2, 7.6f}), 0xF2C14E, 0xC99227, 0.8f);
        f.ell(5.4f, 5.9f, 1.2f, 1.2f, 0xC99227);
        break;
    case WOW:
    case POSE:
    case CHEERS:
    case FLOAT:
    case LAUGH:
    case ROLL: {
        Path m;
        m.to(-3.8f, 4).quad(0, 4.6f, 3.8f, 4, 3).quad(3.4f, 9.4f, 0, 9.4f, 3).quad(-3.4f, 9.4f, -3.8f, 4, 3);
        f.fill(m, K_MOUTH);
        break;
    }
    case TIRED:
    case FLAT:
        f.stroke(poly({-3.2f, 6.2f, -1.6f, 5.1f, 0, 6.2f, 1.6f, 5.1f, 3.2f, 6.2f}), 1.3f, K_MOUTH);
        break;
    case SLEEP:
        f.ell(0, 5.6f, 1.3f, 1.6f, K_MOUTH);
        break;
    default:
        curve(-3.3f, 4.4f, 0, 7.8f, 3.3f, 4.4f, 1.4f, K_MOUTH);
    }
    if (mood == COOL) {
        f.ell(-9.6f, 3.4f, 2.3f, 1.3f, mix(K_BASE, K_BLUSH, 0.5f));
        f.ell(9.6f, 3.4f, 2.3f, 1.3f, mix(K_BASE, K_BLUSH, 0.5f));
    } else if (mood != SHOCK) {
        const float a = (mood == FULL || mood == HUG || mood == WOW || mood == POSE) ? 0.9f : 0.45f;
        f.ell(-9.2f, 3, 2.5f, 1.4f, mix(K_BASE, K_BLUSH, a));
        f.ell(9.2f, 3, 2.5f, 1.4f, mix(K_BASE, K_BLUSH, a));
    }
    if (mood == CHEERS) {
        b.fill(poly({15.5f, -36, 22.5f, -36, 21.6f, -25, 16.4f, -25}), 0xF7C948, 0xC9A227, 0.8f);
        b.rect(15.8f, -38.4f, 6.4f, 2.8f, 1.2f, WHITE);
    }
    if (mood == SNOW) {  // bobble hat and scarf
        Path hat;
        hat.to(-13, -24).quad(-12, -38, 0, -39, 4).quad(12, -38, 13, -24, 4);
        b.fill(hat, 0xE5484D);
        b.rect(-14, -26, 28, 5, 2.5f, WHITE);
        b.ell(0, -40, 3.4f, 3.4f, WHITE);
        Path scarf;
        scarf.to(-12, -4).quad(0, 2, 12, -4, 4).to(12, 0).quad(0, 6, -12, 0, 4);
        b.area(scarf, 0x4BA3D9);
        b.fill(poly({6, -1, 9, 10, 13, 9, 10, -2}), 0x4BA3D9);
    }

    // What floats around him does not move with his body.
    if (mood == TIRED || mood == FLAT) {
        const float k = fract(c * 0.7f);
        const Pen d = o.move(mood == FLAT ? 20 : 15 + k * 3, (mood == FLAT ? -14 : -28) + k * 8).scale(0.7f);
        if (1 - k > 0.05f) {
            d.wedge(0, -5, 0, 2.5f, 0.3f, 3, fade(0x6FB8E6, 1 - k));
        }
    }
    if (mood == SHOCK) {
        o.text(font_for(st, 15 * o.k), "!", 0, -36 - fabsf(sinf(c * 6)) * 3, lgfx::baseline_center, 0xE5484D);
        for (int s = -1; s <= 1; s += 2) {
            o.line(s * 13, -36, s * 17, -41, 1.6f, 0xE5484D);
            o.line(s * 17, -30, s * 23, -32, 1.6f, 0xE5484D);
        }
    }
    if (mood == SLEEP) {
        for (int i = 0; i < 3; i++) {
            const float k = fract(c * 0.4f + i / 3.0f), a = sinf(k * PI_F);
            if (a > 0.05f) {
                o.text(font_for(st, (7 + 1.5f * i) * o.k), "z", 10 + k * 12 + i * 2, -30 - k * 22, lgfx::baseline_center,
                       fade(0x7C6BD6, a));
            }
        }
    }
    if (mood == COACH && fmodf(c, 1.2f) < 0.45f) {  // the whistle blows
        for (int i = 0; i < 3; i++) {
            o.line(10, -12 - i * 2, 14 + i, -14 - i * 2 + i * 2, 1.4f, 0xE5484D);
        }
    }
    if (mood == LAUGH) {
        for (int i = 0; i < 2; i++) {
            const float k = fract(c * 0.8f + i / 2.0f), a = sinf(k * PI_F);
            if (a > 0.05f) {
                o.text(font_for(st, 7 * o.k), "ha", i ? 18 : -18, -30 - k * 12, lgfx::baseline_center, fade(0xC77B12, a));
            }
        }
    }
    if (mood == ROLL) {  // speed lines behind him
        for (int i = 0; i < 3; i++) {
            o.line(-22, -26 + i * 9, -34, -26 + i * 9, 2.4f, WHITE);
        }
    }
    if (mood == WOW || mood == POSE) {
        for (int i = 0; i < 2; i++) {
            const float tw = fabsf(sinf(c * 3 + i * 1.7f));
            star(o.move(i ? 20 : -20, i ? -34 : -28).scale(0.3f + tw * 0.45f), 0xFFC93C);
        }
    }
    if (mood == HUG || mood == FULL) {
        const float k = fract(c * 0.5f), a = sinf(k * PI_F);
        if (a > 0.05f) {
            Path heart;
            heart.to(0, -2).cubic(-3, -7, -9, -5, -7, 0, 5).to(0, 6).to(7, 0).cubic(9, -5, 3, -7, 0, -2, 5);
            o.move(14 + sinf(k * 6) * 2, -36 - k * 16).scale(0.75f).fill(heart, fade(0xF06C8B, a));
        }
    }
}

void peaks(const Pen &p, const Palette &pal)
{
    p.area(poly({-120, -30, -94, -86, -80, -76, -62, -116, -44, -92, -28, -104, -10, -66, 8, -108, 22, -134, 36, -112, 50, -128,
                 66, -96, 84, -118, 102, -84, 120, -102, 150, -54, 150, 0, -120, 0}),
           pal.rock, pal.edge, 1.6f);
    static const float SHADE[5][6] = {{-62, -116, -56, -80, -44, -92}, {22, -134, 28, -96, 36, -112}, {50, -128, 56, -98, 66, -96},
                                      {84, -118, 90, -90, 102, -84},   {-28, -104, -22, -76, -10, -66}};
    for (auto &s : SHADE) {
        p.fill(poly({s[0], s[1], s[2], s[3], s[4], s[5]}), pal.shade);
    }
    p.fill(poly({-66, -108, -62, -116, -57, -106, -61, -108}), pal.snow);
    p.fill(poly({18, -126, 22, -134, 27, -124, 22, -127}), pal.snow);
    p.fill(poly({47, -122, 50, -128, 54, -120}), pal.snow);
    p.fill(poly({81, -112, 84, -118, 88, -110}), pal.snow);
    Path hill;
    hill.to(-120, -26).quad(-70, -50, -16, -30, 8).quad(36, -12, 82, -38, 8).quad(118, -56, 150, -34, 6).to(150, 0).to(-120, 0);
    p.area(hill, pal.hill);
    Path hill2;
    hill2.to(-120, -8).quad(-50, -24, 24, -12, 8).quad(86, -2, 150, -16, 8).to(150, 0).to(-120, 0);
    p.area(hill2, pal.hill2);
}

void fir(const Pen &p, float x, float y, float s, uint32_t col)
{
    p.rect(x - 1.5f * s, y - 5 * s, 3 * s, 5 * s, 0, 0x8A5A3B);
    p.area(poly({x, y - 30 * s, x + 9 * s, y - 14 * s, x + 5 * s, y - 14 * s, x + 11 * s, y - 4 * s, x - 11 * s, y - 4 * s,
                 x - 5 * s, y - 14 * s, x - 9 * s, y - 14 * s}),
           col);
}

void tree(const Pen &p, float x, float y, float s, uint32_t col)
{
    p.rect(x - 2.5f * s, y - 16 * s, 5 * s, 16 * s, 0, 0x8A5A3B);
    p.ell(x, y - 30 * s, 13 * s, 13 * s, col);
    p.ell(x - 9 * s, y - 22 * s, 10 * s, 10 * s, col);
    p.ell(x + 9 * s, y - 22 * s, 10 * s, 10 * s, col);
}

void table_cloth(const Pen &p)
{
    // The pen is not turned here: the checks are plain screen rectangles.
    const auto box = [&](float x, float y, float w, float h, uint32_t col) {
        const int x0 = (int)lroundf(p.X(x, y)), y0 = (int)lroundf(p.Y(x, y));
        const int x1 = (int)lroundf(p.X(x + w, y + h)), y1 = (int)lroundf(p.Y(x + w, y + h));
        p.g->fillRect(x0, y0, x1 - x0, y1 - y0, col);
    };
    box(-120, -36, 280, 80, WHITE);
    const uint32_t red = mix(WHITE, 0xE5484D, 0.82f);
    for (int i = 0; i < 28; i++) {
        for (int j = 0; j < 8; j++) {
            if ((i + j) % 2 == 0) {
                box(-120 + i * 10, -36 + j * 10, 10, 10, red);
            }
        }
    }
    box(-120, -38, 280, 4, mix(WHITE, 0xB8323A, 0.35f));
}

}  // namespace scenery
