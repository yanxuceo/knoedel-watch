#include "dinge.h"

#include <initializer_list>

#include "gfx_util.h"
#include "scenery.h"
#include "vec.h"

using namespace gfxu;
using namespace character;
using vec::Path;
using vec::Pen;

namespace dinge {

namespace {

constexpr float PI_F = 3.14159265f;
constexpr uint32_t WHITE = 0xFFFFFF, INK = 0x3B2F4A;

float ease(float u) { return -(cosf(PI_F * u) - 1.0f) / 2.0f; }
float fract(float v) { return v - floorf(v); }
float loop(float c, float period) { return fract(c / period); }  // 0..1 sawtooth
// 1 between a and b, fading in and out over `fade`.
float window(float u, float a, float b, float fade) { return clampf((u - a) / fade) * clampf((b - u) / fade); }

// One object on the 200 x 200 disc, in the units of dinge.js. The pen only
// moves and scales (no turn), so scan lines can be computed in disc units.
struct Ctx {
    const Stage *st;
    Pen p;
    float c;

    uint32_t fade(uint32_t color, float a, uint32_t under) const { return mix(under, color, a); }
    uint32_t fade(uint32_t color, float a) const { return mix(st->bg, color, a); }
    // Text never scales: the font is the stage's nearest to the drawing's size.
    void text(const Pen &q, const char *s, float x, float y, float size, uint32_t col,
              lgfx::textdatum_t datum = lgfx::middle_center) const
    {
        const float px = size * q.k;
        q.text(px < 17 ? st->font_tiny : px < 36 ? st->font_small : st->font_big, s, x, y, datum, col);
    }
    // A sheen sweeping across a shape: the band between x = left(y) and
    // x = left(y) + width, cut to [lo(y), hi(y)], rows y0..y1 (disc units).
    template <typename Cut>
    void sheen(float y0, float y1, float x_top, float slant, float width, float alpha, Cut cut) const
    {
        const int r0 = (int)floorf(p.Y(0, y0)), r1 = (int)ceilf(p.Y(0, y1));
        for (int row = r0; row < r1; row++) {
            const float y = (row + 0.5f - p.f) / p.d;
            if (y < y0 || y > y1) {
                continue;
            }
            float lo, hi;
            if (!cut(y, lo, hi)) {
                continue;
            }
            const float l = fmaxf(lo, x_top + slant * (y - y0)), r = fminf(hi, x_top + slant * (y - y0) + width);
            const int xl = (int)lroundf(p.X(l, y)), xr = (int)lroundf(p.X(r, y));
            span(p.g, xl, row, xr - xl, alpha, WHITE);
        }
    }
};

Path poly(std::initializer_list<float> xy)
{
    Path q;
    for (auto i = xy.begin(); i != xy.end(); i += 2) {
        q.to(i[0], i[1]);
    }
    return q;
}

// Three wisps rising from (x, y), spaced by dx: steam, heat.
void wisps(const Ctx &x, int n, float x0, float dx, float y0, float rise, float speed, float h, float w, uint32_t col)
{
    for (int i = 0; i < n; i++) {
        const float k = fract(x.c * speed + (float)i / n), a = sinf(PI_F * k);
        if (a < 0.05f) {
            continue;
        }
        Path wisp;
        wisp.to(0, 0).quad(-h * 0.75f, -h, 0, -2 * h, 4).quad(h * 0.75f, -3 * h, 0, -4 * h, 4);
        x.p.move(x0 + i * dx, y0 - k * rise).stroke(wisp, w, x.fade(col, a));
    }
}

// ---- a2-wohnung-nomen ---------------------------------------------------------------

// A cushion bouncing on the sofa.
void o_sofa(const Ctx &x)
{
    const Pen &p = x.p;
    p.ell(100, 160, 70, 5, 0x000000, 0.07f);
    p.rect(46, 150, 6, 10, 1.5f, 0x7A4A2E);
    p.rect(148, 150, 6, 10, 1.5f, 0x7A4A2E);
    p.rect(40, 88, 120, 44, 15, 0x9D8DF1, 0x7C6BD6, 2.2f);
    p.line(100, 94, 100, 126, 1.8f, 0x7C6BD6);
    p.rect(38, 118, 124, 32, 10, 0xB3A6F5, 0x7C6BD6, 2.2f);
    p.line(100, 120, 100, 148, 1.8f, 0x7C6BD6);
    p.rect(26, 104, 22, 48, 10, 0x9D8DF1, 0x7C6BD6, 2.2f);
    p.rect(152, 104, 22, 48, 10, 0x9D8DF1, 0x7C6BD6, 2.2f);
    const float k = fabsf(sinf(x.c * 2.6f)), b = k * k * k;
    const Pen pil = p.move(70, 108 - b * 10).turn(-14 + b * 10).scale(1 + (1 - k) * 0.08f, 1 - (1 - k) * 0.1f);
    pil.rect(-13, -13, 26, 26, 7, 0xF2C14E, 0xC99227, 1.8f);
    pil.ell(0, 0, 2.4f, 2.4f, 0xC99227);
}

// A rug with a small cat asleep on it.
void o_teppich(const Ctx &x)
{
    const Pen &p = x.p;
    p.fill(poly({52, 112, 168, 112, 148, 164, 32, 164}), 0xE5873A, 0xB8652A, 2);
    p.stroke(poly({62, 120, 154, 120, 139, 156, 47, 156}), 2.2f, 0xFFE3C2, true);
    p.fill(poly({78, 138, 100, 128, 122, 138, 100, 148}), 0xF6C356);
    for (int i = 0; i < 6; i++) {
        const float y = 112 + i * 9.5f;
        p.line(52 - i * 3.6f, y, 45 - i * 3.6f, y, 1.6f, 0xB8652A);
        p.line(168 - i * 3.6f, y, 175 - i * 3.6f, y, 1.6f, 0xB8652A);
    }
    const Pen cat = p.move(98, 124);
    Path tail;
    tail.to(14, 4).cubic(30, 2, 30, -14, 20, -16);
    cat.stroke(tail, 7, 0xC9772F);
    cat.stroke(tail, 3.6f, 0xF3A25A);
    cat.ell(0, -2, 22, 12 + sinf(x.c * 2) * 0.8f, 0xF3A25A, 0xC9772F, 1.8f);
    cat.fill(poly({-20, -8, -22, -22, -12, -12}), 0xF3A25A, 0xC9772F, 1.6f);
    cat.fill(poly({-8, -12, -4, -24, 2, -12}), 0xF3A25A, 0xC9772F, 1.6f);
    for (float ex : {-16.0f, -6.0f}) {
        Path eye;
        eye.to(ex, -3).quad(ex + 2.5f, -1, ex + 5, -3, 4);
        cat.stroke(eye, 1.5f, 0x2B2230);
    }
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.45f + i / 3.0f), a = sinf(k * PI_F);
        if (a > 0.05f) {
            x.text(p, "z", 92 + k * 14 + i * 3, 104 - k * 28, 8 + 2 * i, x.fade(0x7C6BD6, a), lgfx::baseline_left);
        }
    }
}

// A broken lamp: it flickers.
void o_lampe(const Ctx &x)
{
    const Pen &p = x.p;
    const float u = loop(x.c, 2.4f);
    const bool on = u < 0.18f || (u > 0.26f && u < 0.3f) || (u > 0.55f && u < 0.62f);
    p.line(100, 0, 100, 58, 2, INK);
    if (on) {
        p.fill(poly({84, 80, 116, 80, 150, 150, 50, 150}), x.fade(0xFFE58A, 0.55f));
    }
    p.rect(52, 150, 96, 7, 3, 0xA86B48);
    p.line(60, 157, 58, 184, 4, 0xA86B48);
    p.line(140, 157, 142, 184, 4, 0xA86B48);
    Path bowl;
    bowl.to(100, 136).quad(92, 135, 92, 143).to(108, 143).quad(108, 135, 100, 136);
    p.fill(bowl, 0xE5484D);
    p.ell(100, 80, 7, 7, on ? 0xFFF7C2 : 0xE9E2C8);
    Path shade;
    shade.to(74, 80).quad(76, 56, 100, 56).quad(124, 56, 126, 80);
    p.fill(shade, 0x4BA3D9, 0x2F7DB8, 2);
    if (!on) {
        p.stroke(poly({132, 70, 126, 80, 132, 80, 126, 90}), 2.2f, x.fade(0xE5484D, 0.9f));
    }
}

// A shelf; one book is pushed in.
void o_regal(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(54, 38, 92, 134, 3, 0xF3E3CF, 0xA86B48, 3);
    p.rect(54, 80, 92, 5, 0, 0xA86B48);
    p.rect(54, 122, 92, 5, 0, 0xA86B48);
    static const float TOP[5][4] = {{60, 9, 32, 0xE5484D}, {70, 7, 36, 0x4BA3D9}, {78, 10, 30, 0xF2C14E},
                                    {89, 8, 34, 0x5E9E45}, {98, 9, 28, 0x9C7AD6}};
    for (auto &b : TOP) {
        p.rect(b[0], 80 - b[2], b[1], b[2], 1.5f, (uint32_t)b[3]);
    }
    p.fill(poly({118, 80, 130, 80, 128, 66, 120, 66}), 0xD9824A);
    Path leaf1, leaf2;
    leaf1.to(124, 66).quad(116, 54, 120, 48);
    leaf2.to(124, 66).quad(132, 54, 128, 46);
    p.stroke(leaf1, 3, 0x5E9E45);
    p.stroke(leaf2, 3, 0x5E9E45);
    static const float LOW[4][4] = {{62, 10, 34, 0x2F7DB8}, {73, 8, 30, 0xE5873A}, {82, 9, 36, 0x7A4FD6}, {92, 11, 32, 0xE5484D}};
    for (auto &b : LOW) {
        p.rect(b[0], 122 - b[2], b[1], b[2], 1.5f, (uint32_t)b[3]);
    }
    const float k = ease(window(loop(x.c, 4), 0.15f, 0.75f, 0.15f));
    p.rect(134 - k * 26, 122 - 33, 9, 33, 1.5f, 0x3E8E2F);
    p.rect(62, 152, 46, 14, 2, 0xC9CED6);
    p.rect(110, 146, 28, 20, 2, 0xF7B9C6);
}

// A wardrobe; its right door opens and shuts.
void o_schrank(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(56, 28, 88, 144, 5, 0xC99A6B, 0x8C5B3A, 2.4f);
    p.rect(60, 34, 80, 128, 2, 0x6B4423);
    p.line(62, 46, 138, 46, 2.4f, 0x9AA4B1);
    p.line(108, 46, 108, 50, 1.6f, 0xC9CED6);
    p.stroke(poly({100, 58, 108, 50, 116, 58}), 1.6f, 0xC9CED6);
    // The shirt in three convex parts, then its outline.
    p.fill(poly({100, 58, 102, 54, 108, 57, 114, 54, 116, 58, 116, 96, 100, 96}), 0x4A6FA5);
    p.fill(poly({96, 58, 102, 54, 100, 64, 98, 66}), 0x4A6FA5);
    p.fill(poly({114, 54, 120, 58, 118, 66, 116, 64}), 0x4A6FA5);
    p.stroke(poly({96, 58, 102, 54, 108, 57, 114, 54, 120, 58, 118, 66, 116, 64, 116, 96, 100, 96, 100, 64, 98, 66}), 1.4f,
             0x33507A, true);
    p.fill(poly({124, 58, 132, 54, 138, 58, 136, 92, 126, 92}), 0xE5484D);
    p.rect(56, 172, 10, 6, 0, 0x8C5B3A);
    p.rect(134, 172, 10, 6, 0, 0x8C5B3A);
    p.rect(58, 30, 42, 140, 3, 0xD9AE7E, 0x8C5B3A, 2);
    p.ell(94, 100, 2.6f, 2.6f, 0xF2C14E);
    const float k = ease(window(loop(x.c, 5), 0.15f, 0.8f, 0.15f));
    const Pen door = p.move(142, 30).scale(1 - 0.85f * k, 1);
    door.rect(-42, 0, 42, 140, 3, 0xD9AE7E, 0x8C5B3A, 2);
    door.stroke(Path::rect(-36, 12, 30, 50, 2), 1.4f, 0xB88A5E, true);
    door.oval(-36, 70, 2.6f, 2.6f, 0xF2C14E);
}

// A bathroom mirror; light glides across the glass.
void o_spiegel(const Ctx &x)
{
    const Pen &p = x.p;
    const uint32_t tile = x.fade(WHITE, 0.6f);
    for (int v = 20; v < 190; v += 20) {
        p.line(v, 0, v, 200, 1.2f, tile);
    }
    for (int h = 20; h < 200; h += 20) {
        p.line(0, h, 200, h, 1.2f, tile);
    }
    Path basin;
    basin.to(62, 150).to(138, 150).quad(136, 172, 100, 172).quad(64, 172, 62, 150);
    p.fill(basin, WHITE, 0xB9BFCC, 2);
    Path tap;
    tap.to(100, 150).to(100, 142).quad(100, 138, 106, 138);
    p.stroke(tap, 3, 0x9AA4B1);
    p.ell(100, 84, 38, 50, 0xC99A6B);
    p.ell(100, 84, 34, 46, 0xDDF1FB);
    // The sheen M0 0 L12 0 L-28 90 L-40 90 Z at (60 + 110 u, 36), inside the glass.
    x.sheen(36, 126, 60 + loop(x.c, 3) * 110, -40.0f / 90, 12, 0.85f, [](float y, float &lo, float &hi) {
        const float n = (y - 84) / 46;
        if (fabsf(n) >= 1) {
            return false;
        }
        const float hw = 34 * sqrtf(1 - n * n);
        lo = 100 - hw;
        hi = 100 + hw;
        return true;
    });
}

// A cooker: the oven glows, the pot steams, a hot plate shimmers.
void o_herd(const Ctx &x)
{
    const Pen &p = x.p;
    const float pulse = 0.5f + 0.5f * sinf(x.c * 3);
    p.rect(52, 92, 96, 82, 5, 0xF6F8FB, 0xAEB6C4, 2.2f);
    p.rect(52, 92, 96, 12, 3, 0xD6DCE6);
    for (float kx : {64.0f, 80.0f, 120.0f, 136.0f}) {
        p.ell(kx, 98, 3, 3, 0x6B7180);
    }
    p.rect(62, 112, 76, 50, 5, INK);
    p.rect(66, 116, 68, 42, 4, x.fade(0xF28C28, 0.35f + 0.3f * pulse, INK));
    p.rect(70, 106, 60, 4, 2, 0x9AA4B1);
    p.ell(76, 90, 16, 3, INK);
    p.stroke(Path::oval(124, 90, 14, 3, 20), 2.4f, pulse > 0.5f ? 0xFF6B4A : 0xE5484D, true);
    Path pot;
    pot.to(62, 88).to(62, 66).quad(62, 62, 66, 62).to(86, 62).quad(90, 62, 90, 66).to(90, 88);
    p.fill(pot, 0x4BA3D9, 0x2F7DB8, 1.8f);
    p.line(58, 64, 94, 64, 3, 0x2F7DB8);
    wisps(x, 3, 68, 7, 58, 18, 0.6f, 4, 2, 0xC9D3DE);
    wisps(x, 3, 116, 8, 84, 16, 0.8f, 3, 1.8f, 0xF28C28);
}

// A radiator giving off heat.
void o_heizung(const Ctx &x)
{
    const Pen &p = x.p;
    p.line(40, 150, 40, 168, 4, 0x9AA4B1);
    p.line(160, 150, 160, 168, 4, 0x9AA4B1);
    for (int i = 0; i < 7; i++) {
        p.rect(44 + i * 16, 98, 12, 62, 6, 0xF6F8FB, 0xAEB6C4, 1.8f);
    }
    p.rect(40, 150, 120, 6, 3, 0xD6DCE6);
    p.ell(158, 104, 7, 7, 0xF6F8FB, 0xAEB6C4, 1.8f);
    x.text(p, "5", 158, 104.5f, 7, 0xE5484D);
    wisps(x, 4, 62, 25, 92, 30, 0.5f, 5, 2.2f, 0xF28C28);
}

// A socket; a plug goes in, and it sparks.
void o_steckdose(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(62, 60, 76, 76, 14, WHITE, 0xC9CED6, 2.4f);
    p.ell(100, 98, 26, 26, 0xF3F5F8, 0xD6DCE6, 2);
    p.ell(90, 98, 3.6f, 3.6f, INK);
    p.ell(110, 98, 3.6f, 3.6f, INK);
    const float k = ease(window(loop(x.c, 4), 0.1f, 0.75f, 0.15f));
    const Pen plug = p.move(150 - k * 28, 132 - k * 34);
    Path cable;
    cable.to(0, 0).quad(30, 20, 70, 10, 8);
    plug.stroke(cable, 4, INK);
    plug.rect(-14, -11, 18, 22, 6, INK);
    plug.rect(-22, -6.4f, 9, 2.8f, 1.2f, 0xC9CED6);
    plug.rect(-22, 3.6f, 9, 2.8f, 1.2f, 0xC9CED6);
    if (k > 0.95f) {
        p.stroke(poly({150, 56, 142, 70, 150, 70, 142, 84}), 3, x.fade(0xF2C14E, 0.5f + 0.5f * sinf(x.c * 12)));
    }
}

// ---- a2-gesundheit-nomen ------------------------------------------------------------

// A doctor's coat with a stethoscope; the heart beats, the ECG runs.
void o_arzt(const Ctx &x)
{
    const Pen &p = x.p;
    Path coat;
    coat.to(48, 176).to(56, 104).quad(60, 92, 76, 90).to(124, 90).quad(140, 92, 144, 104).to(152, 176);
    p.fill(coat, WHITE, 0xC9CED6, 2);
    p.fill(poly({86, 90, 100, 122, 114, 90}), 0xBFE3F2);
    p.stroke(poly({86, 90, 94, 130, 100, 122}), 2, 0xC9CED6);
    p.stroke(poly({114, 90, 106, 130, 100, 122}), 2, 0xC9CED6);
    p.rect(120, 132, 18, 4, 2, 0x4BA3D9);
    Path tube1, tube2;
    tube1.to(82, 92).quad(72, 128, 92, 140, 8).quad(104, 146, 112, 136);
    tube2.to(118, 92).quad(124, 118, 112, 136, 6);
    p.stroke(tube1, 3, INK);
    p.stroke(tube2, 3, INK);
    const Pen chest = p.move(112, 140 + sinf(x.c * 2) * 2);
    chest.ell(0, 0, 7, 7, 0x9AA4B1, 0x6B7180, 2);
    chest.ell(0, 0, 3, 3, 0xD6DCE6);
    const float b = powf(fmaxf(0, sinf(x.c * 5)), 6);
    Path heart;
    heart.to(0, -2).cubic(-3, -7, -9, -5, -7, 0, 5).to(0, 6).to(7, 0).cubic(9, -5, 3, -7, 0, -2, 5);
    p.move(140, 108).scale(1.1f + b * 0.4f).fill(heart, 0xF06C8B);
    // The ECG line is drawn up to `shown` along its length.
    static const float ECG[7][2] = {{40, 62}, {70, 62}, {76, 50}, {84, 74}, {90, 56}, {94, 62}, {160, 62}};
    float shown = 140 * loop(x.c, 1.6f), done = 0;
    for (int i = 0; i + 1 < 7 && done < shown; i++) {
        const float len = hypotf(ECG[i + 1][0] - ECG[i][0], ECG[i + 1][1] - ECG[i][1]), t = fminf(1, (shown - done) / len);
        p.line(ECG[i][0], ECG[i][1], ECG[i][0] + (ECG[i + 1][0] - ECG[i][0]) * t, ECG[i][1] + (ECG[i + 1][1] - ECG[i][1]) * t,
               2.4f, 0xE5484D);
        done += len;
    }
}

// A surgery door with its brass plate and opening hours. Plate and hours are
// larger than in the preview, for the watch's smallest font, and the plate
// leaves out the doctor's name.
void o_praxis(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(30, 30, 140, 150, 0, 0xF3E9DC);
    p.rect(70, 70, 60, 110, 3, 0xB07A4E, 0x7A4A2E, 2);
    p.rect(78, 80, 44, 34, 2, 0xBFE3F2);
    p.ell(120, 130, 3, 3, 0xF2C14E);
    p.rect(132, 76, 38, 20, 2, 0xE4C27A, 0xB08A3E, 1.4f);
    x.text(p, "Praxis", 151, 86, 5.5f, 0x5B4214);
    const float u = loop(x.c, 3);
    x.sheen(76.7f, 95.3f, 132 + u * 40, -10.0f / 24, 6, 0.7f * window(u, 0, 1, 0.2f), [](float, float &lo, float &hi) {
        lo = 132.7f;
        hi = 169.3f;
        return true;
    });
    p.rect(32, 76, 34, 42, 2, WHITE, 0xC9CED6, 1);
    x.text(p, "Mo–Fr", 49, 85, 5, INK);
    for (float y : {98.0f, 104.0f, 110.0f}) {
        p.line(37, y, 61, y, 1.2f, 0xC9CED6);
    }
    p.line(37, 104, 61, 104, 2, x.fade(0xE5484D, sinf(x.c * 3) > 0 ? 0.9f : 0.2f, WHITE));
}

// A health insurance card, tilting.
void o_krankenkasse(const Ctx &x)
{
    const float s = sinf(x.c * 1.4f);
    const Pen card = x.p.move(100, 104).turn(-8 + s * 6).scale(1 + s * 0.03f);
    card.rect(-48, -30, 96, 60, 7, 0x4BA3D9, 0x2F7DB8, 2);
    card.rect(-47, 8, 94, 10, 0, 0x2F7DB8);
    card.rect(-38, -18, 16, 12, 2, 0xF2C14E, 0xC99227, 1);
    card.line(-38, -12, -22, -12, 1, 0xC99227);
    card.line(-30, -18, -30, -6, 1, 0xC99227);
    card.rect(14, -22, 24, 26, 3, 0xDDF1FB);
    card.oval(26, -12, 5, 5, 0x9AA4B1);
    Path shoulders;
    shoulders.to(17, 4).quad(26, -6, 35, 4);
    card.fill(shoulders, 0x9AA4B1);
    const uint32_t ink = x.fade(WHITE, 0.8f, 0x4BA3D9);
    card.line(-38, 0, 4, 0, 2, ink);
    card.line(-38, -4, 0, -4, 2, ink);
    Path heart;
    heart.to(-4, 12).cubic(-6, 9, -10, 10, -9, 13, 4).to(-4, 17).to(1, 13).cubic(2, 10, -2, 9, -4, 12, 4);
    card.fill(heart, WHITE);
}

// A prescription, stamped.
void o_rezept(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(50, 52, 100, 110, 3, 0xF7C6D3, 0xD9708E, 2);
    x.text(p, "Rp.", 60, 70, 11, 0xB0506C, lgfx::baseline_left);
    for (int i = 0; i < 4; i++) {
        p.line(60, 86 + i * 12, 140 - i * 12, 86 + i * 12, 1.6f, 0xD9708E);
    }
    p.stroke(Path::rect(60, 132, 40, 20, 2), 1.4f, 0xD9708E, true);
    const float u = loop(x.c, 3), down = window(u, 0.3f, 0.42f, 0.06f);
    if (u > 0.36f) {
        p.ring(124, 142, 12, 2, 0x2F5DA8);
        p.line(118, 142, 130, 142, 1.6f, 0x2F5DA8);
        p.line(124, 136, 124, 148, 1.6f, 0x2F5DA8);
    }
    const Pen stamp = p.move(124, 118 - 34 * (1 - down) + 18);
    stamp.rect(-10, -6, 20, 8, 2, INK);
    stamp.rect(-4, -24, 8, 18, 3, 0xA86B48);
    stamp.ell(0, -27, 6, 6, 0xA86B48);
}

// A pharmacy with its red sign swinging.
void o_apotheke(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(30, 100, 140, 80, 0, 0xF3F4F6);
    p.rect(44, 116, 50, 50, 2, 0xBFE3F2, 0x9AA4B1, 1.6f);
    p.rect(110, 116, 40, 64, 2, 0x6B7180);
    p.rect(56, 140, 10, 18, 2, 0xE5873A);
    p.rect(72, 146, 14, 12, 1.5f, WHITE);
    p.line(100, 40, 132, 40, 3, 0x6B7180);
    const Pen sign = p.move(116, 40).turn(sinf(x.c * 1.6f) * 6);
    sign.line(-8, 0, -8, 10, 1.6f, 0x6B7180);
    sign.line(8, 0, 8, 10, 1.6f, 0x6B7180);
    sign.rect(-20, 10, 40, 44, 6, 0xE5484D);
    x.text(sign, "A", 0, 33, 32, WHITE);
}

// A bottle of medicine, rocking, beside its box.
void o_medikament(const Ctx &x)
{
    const Pen &p = x.p;
    p.rect(104, 112, 52, 52, 3, WHITE, 0xC9CED6, 2);
    p.rect(105, 126, 50, 12, 0, 0x4BA3D9);
    x.text(p, "20 St.", 130, 151, 7, 0x6B7180);
    const Pen bottle = p.move(80, 164).turn(sinf(x.c * 2.2f) * 4);
    bottle.rect(-14, -54, 28, 14, 3, WHITE, 0xC9CED6, 1.6f);
    bottle.rect(-20, -42, 40, 46, 5, 0xC97A2E, 0x9B5A1E, 1.6f);
    bottle.rect(-16, -30, 32, 20, 2, WHITE);
    bottle.rect(-9, -24, 18, 8, 4, 0xF06C8B);
    bottle.rect(0, -24, 9, 8, 4, WHITE, 0xF06C8B, 1);
    const float tw = fabsf(sinf(x.c * 2));
    if (tw > 0.05f) {
        const Pen sp = p.move(58, 108).scale(0.4f + tw * 0.7f);
        sp.fill(poly({0, -6, 1.6f, -1.6f, 6, 0, 1.6f, 1.6f, 0, 6, -1.6f, 1.6f, -6, 0, -1.6f, -1.6f}), x.fade(0xFFC93C, tw));
    }
}

// A blister of tablets: one jumps into the glass of water.
void o_tablette(const Ctx &x)
{
    const Pen &p = x.p;
    const float u = loop(x.c, 3), k = clampf((u - 0.2f) / 0.5f);
    p.rect(46, 70, 84, 62, 6, 0xD6DCE6, 0x9AA4B1, 2);
    for (int r = 0; r < 2; r++) {
        for (int i = 0; i < 4; i++) {
            const float cx = 58 + i * 20, cy = 88 + r * 26;
            p.ell(cx, cy, 8, 9, 0xEEF0F4, 0xB9BFCC, 1.4f);
            if (!(r == 1 && i == 3 && u > 0.2f)) {
                p.ell(cx, cy, 5, 6, WHITE, 0xC9CED6, 1);
            }
        }
    }
    p.fill(poly({140, 120, 164, 120, 161, 164, 143, 164}), 0xDDF1FB, 0x9AA4B1, 2);
    p.fill(poly({142, 134, 162, 134, 161, 162, 143, 162}), x.fade(0x9FD3EE, 0.8f, 0xDDF1FB));
    if (u > 0.2f && u < 0.75f) {
        p.ell(118 + k * 34, 114 - sinf(PI_F * k) * 26 + k * 16, 5, 6, WHITE, 0xC9CED6, 1);
    }
}

// ---- a2-berg-nomen --------------------------------------------------------------------

// A small flower nodding, its foot at the origin of `q`.
void flower(const Pen &q, float stem, float rx, float ry, float centre, uint32_t col)
{
    q.line(0, 0, 0, stem, 1.6f, 0x5E9E45);
    for (int a = 0; a < 360; a += 72) {
        q.turn(a).oval(0, -3.2f * ry / 3.4f, rx, ry, col);
    }
    q.ell(0, 0, centre, centre, 0xF2A93B);
}

// A mountain with its morning glow and a passing cloud.
void o_berg(const Ctx &x)
{
    const Pen &p = x.p;
    const Pen m = p.move(100, 158).scale(0.78f);
    const Path range = poly({-120, 0, -96, -60, -84, -52, -66, -104, -50, -80, -34, -118, -18, -150, -2, -122, 12, -136, 30, -96,
                             46, -112, 64, -70, 80, -84, 120, 0});
    m.area(range, 0xEEE7DD, 0xC9BBA8, 2);
    m.area(range, x.fade(0xF59AA8, 0.28f * (0.5f + 0.5f * sinf(x.c * 0.9f)), 0xEEE7DD));
    const uint32_t shade = mix(0xEEE7DD, 0xDDD2C2, 0.9f);
    m.fill(poly({-18, -150, -12, -104, -2, -122}), shade);
    m.fill(poly({-66, -104, -60, -70, -50, -80}), shade);
    m.fill(poly({12, -136, 18, -100, 30, -96}), shade);
    m.fill(poly({46, -112, 52, -78, 64, -70}), shade);
    m.fill(poly({-24, -140, -18, -150, -12, -138, -18, -141}), WHITE);
    m.fill(poly({8, -128, 12, -136, 17, -126}), WHITE);
    m.fill(poly({-70, -96, -66, -104, -61, -94}), WHITE);
    Path hill, hill2;
    hill.to(-120, -6).quad(-60, -34, 0, -16, 8).quad(60, 0, 120, -22, 8).to(120, 30).to(-120, 30);
    hill2.to(-120, 10).quad(-30, -6, 40, 6, 8).quad(90, 14, 120, 4, 6).to(120, 30).to(-120, 30);
    m.area(hill, 0xA8D08D);
    m.area(hill2, 0x8CC06E);
    scenery::fir(m, -70, -6, 0.9f);
    scenery::fir(m, -54, -2, 0.7f, 0x4E9E68);
    scenery::fir(m, 70, -4, 0.8f);
    const Pen cloud = p.move(40 + fmodf(x.c * 9, 140), 62);
    const uint32_t white = x.fade(WHITE, 0.95f);
    cloud.ell(0, 0, 9, 9, white);
    cloud.ell(10, 2, 7, 7, white);
    cloud.ell(-9, 3, 6, 6, white);
}

// One hiking boot, its heel at the origin of `b`.
void boot(const Ctx &x, const Pen &b, uint32_t col, float bow_turn)
{
    Path upper;
    upper.to(0, 0).to(0, -54).quad(0, -60, 6, -60, 3).to(30, -60).quad(36, -60, 36, -54, 3).to(36, -28).quad(60, -24, 66, -12, 5).to(66, 0);
    b.area(upper, col, 0x6E4528, 2);
    Path sole;
    sole.to(-2, 0).to(68, 0).to(68, 8).quad(68, 10, 66, 10, 2).to(0, 10).quad(-2, 10, -2, 8, 2);
    b.fill(sole, 0x3B2F4A);
    for (int i = 0; i < 8; i++) {
        b.rect(2 + i * 8.4f, 8, 4, 4, 0, 0x3B2F4A);
    }
    Path seam;
    seam.to(36, -26).quad(44, -20, 52, -18, 4);
    b.stroke(seam, 1.6f, 0x6E4528);
    b.line(0, -12, 66, -12, 1.4f, mix(col, 0x6E4528, 0.6f));
    b.rect(-1, -62, 38, 8, 3, 0xE5484D);
    for (int yy = -48; yy <= -32; yy += 8) {
        b.line(26, yy, 36, yy + 3, 2, 0xF2C14E);
    }
    Path loops;
    loops.to(36, -30).quad(44, -38, 46, -30, 3).quad(44, -24, 36, -30, 3).quad(30, -40, 26, -32, 3).quad(30, -24, 36, -30, 3);
    b.turn(bow_turn, 36, -30).area(loops, 0xF2C14E);
    (void)x;
}

// A pair of hiking boots; the front one taps.
void o_wanderschuh(const Ctx &x)
{
    const Pen &p = x.p;
    p.ell(100, 156, 64, 5, 0x000000, 0.08f);
    boot(x, p.move(76, 146), 0xB07A4F, sinf(x.c * 4) * 6);
    const float k = fmaxf(0, sinf(x.c * 3)), t = k * k;
    boot(x, p.move(46, 154 - t * 8).turn(-t * 6), 0xC68B59, 0);
}

// A trail to the hut, footsteps appearing one by one.
void o_weg(const Ctx &x)
{
    const Pen &p = x.p;
    scenery::peaks(p.move(100, 124).scale(0.82f, 0.55f), scenery::DAY);
    Path trail;
    trail.to(20, 200).quad(30, 136, 96, 120, 8).quad(134, 110, 112, 102, 6).quad(98, 96, 108, 90, 4);
    p.stroke(trail, 12, 0xE8D3A8);
    const float u = loop(x.c, 3.6f);
    static const float STEPS[6][2] = {{30, 176}, {44, 150}, {64, 132}, {92, 116}, {122, 102}, {140, 86}};
    for (int i = 0; i < 6; i++) {
        const float a = window(u, i * 0.1f, i * 0.1f + 0.45f, 0.06f) * 0.85f;
        if (a > 0.05f) {
            p.ell(STEPS[i][0] + (i % 2 ? 4 : -4), STEPS[i][1], 3.4f - i * 0.3f, 2 - i * 0.15f, x.fade(0xB0915E, a, 0xE8D3A8));
        }
    }
    p.rect(140, 90, 6, 80, 0, 0xA86B48);
    const Pen sign = p.turn(sinf(x.c * 1.5f) * 1.5f, 143, 106);
    sign.fill(poly({108, 96, 168, 96, 178, 106, 168, 116, 108, 116}), WHITE, 0xC9CED6, 1.4f);
    sign.fill(poly({168, 96, 178, 106, 168, 116}), 0xE5484D);
    x.text(sign, "Hütte", 112, 110, 9, 0x2B2230, lgfx::baseline_left);
    x.text(sign, "1 h", 164, 110, 9, 0x2B2230, lgfx::baseline_right);
    Path stone;
    stone.to(30, 152).quad(36, 140, 50, 142, 4).quad(58, 136, 64, 146, 4).quad(70, 154, 60, 160, 4).to(34, 160).quad(26, 158, 30, 152, 3);
    p.fill(stone, 0xBDB7AE, 0x8E8880, 1.4f);
    p.rect(40, 146, 14, 3, 0, 0xE5484D);
    p.rect(40, 149, 14, 3, 0, WHITE);
    p.rect(40, 152, 14, 3, 0, 0xE5484D);
}

// A forest swaying, a mushroom, a bird crossing.
void o_wald(const Ctx &x)
{
    const Pen &p = x.p;
    Path ground;
    ground.to(0, 150).quad(100, 136, 200, 150, 8).to(200, 200).to(0, 200);
    p.area(ground, 0x8CC06E);
    static const float TREES[5][4] = {{48, 150, 1.5f, 0x4E9E68}, {150, 148, 1.6f, 0x3E8E5A}, {100, 156, 2.2f, 0x3E8E5A},
                                      {72, 162, 1.2f, 0x5BAF74}, {128, 164, 1.25f, 0x4E9E68}};
    for (int i = 0; i < 5; i++) {
        scenery::fir(p.move(TREES[i][0], TREES[i][1]).turn(sinf(x.c * 1.4f + i) * 1.6f), 0, 0, TREES[i][2], (uint32_t)TREES[i][3]);
    }
    const Pen shroom = p.move(150, 170);
    shroom.rect(-2, -6, 4, 7, 1.5f, 0xFFF6DA);
    Path cap;
    cap.to(-7, -5).quad(0, -14, 7, -5, 5);
    shroom.fill(cap, 0xE5484D);
    shroom.ell(-2, -8, 1, 1, WHITE);
    shroom.ell(2.5f, -7, 0.9f, 0.9f, WHITE);
    const float u = loop(x.c, 5);
    const Pen bird = p.move(20 + u * 170, 56 - sinf(u * PI_F) * 14).scale(1, 0.6f + 0.4f * fabsf(sinf(x.c * 9)));
    Path wings;
    wings.to(-6, 0).quad(-3, -4, 0, 0, 3).quad(3, -4, 6, 0, 3);
    bird.stroke(wings, 1.8f, INK);
}

// A meadow with a picnic, nodding flowers and a butterfly.
void o_wiese(const Ctx &x)
{
    const Pen &p = x.p;
    scenery::peaks(p.move(100, 112).scale(0.8f, 0.5f), scenery::DAY);
    Path m1, m2;
    m1.to(0, 118).quad(60, 100, 120, 108, 6).quad(170, 114, 200, 104, 6).to(200, 200).to(0, 200);
    m2.to(0, 150).quad(80, 136, 200, 146, 8).to(200, 200).to(0, 200);
    p.area(m1, 0xA8D08D);
    p.area(m2, 0x8CC06E);
    // The blanket, skewed by -20 degrees.
    Pen bl = p.move(104, 150);
    bl.c += bl.a * tanf(-20 * PI_F / 180);
    bl.d += bl.b * tanf(-20 * PI_F / 180);
    bl.fill(poly({-26, -10, 26, -10, 26, 10, -26, 10}), WHITE);
    const uint32_t red = mix(WHITE, 0xE5484D, 0.85f);
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 2; j++) {
            if ((i + j) % 2 == 0) {
                const float cx = -26 + i * 8.66f, cy = -10 + j * 10.0f;
                bl.fill(poly({cx, cy, cx + 8.66f, cy, cx + 8.66f, cy + 10, cx, cy + 10}), red);
            }
        }
    }
    p.fill(poly({96, 132, 118, 132, 115, 146, 99, 146}), 0xC68B59, 0x8A5A3B, 1.4f);
    Path handle;
    handle.to(100, 132).quad(107, 120, 114, 132, 5);
    p.stroke(handle, 2, 0x8A5A3B);
    static const float FLOWERS[8][3] = {{34, 166, 0xF28CA8}, {52, 176, 0xFFFFFF}, {150, 170, 0xF2C14E}, {168, 160, 0xF28CA8},
                                        {70, 160, 0x9C7AD6}, {134, 180, 0xFFFFFF}, {44, 140, 0xF2C14E}, {160, 138, 0xFFFFFF}};
    for (int i = 0; i < 8; i++) {
        flower(p.move(FLOWERS[i][0], FLOWERS[i][1]).turn(sinf(x.c * 2 + i) * 9), 10, 2.4f, 3.4f, 1.8f, (uint32_t)FLOWERS[i][2]);
    }
    const float k = x.c * 0.7f;
    const Pen bf = p.move(100 + sinf(k) * 44, 96 + sinf(k * 2.3f) * 12).scale(1, 0.55f + 0.45f * fabsf(sinf(x.c * 12)));
    Path wings;
    wings.to(0, 0).cubic(-5, -8, -12, -4, -8, 1, 4).cubic(-10, 5, -4, 8, 0, 1, 4).cubic(4, 8, 10, 5, 8, 1, 4).cubic(12, -4, 5, -8, 0, 0, 4);
    bf.area(wings, 0x9C7AD6);
}

// The cow is drawn in parts, each with its own outlines: in one function
// they took 6 KB of stack.
constexpr uint32_t HIDE = 0xF6F3EE, LINE = 0xB9B4AD;

__attribute__((noinline)) void kuh_spots(const Pen &p)
{
    Path s;
    s.to(70, 102).quad(80, 94, 90, 104, 4).quad(86, 116, 74, 114, 4);
    p.fill(s, INK);
    s.n = 0;
    s.to(114, 112).quad(128, 104, 136, 116, 4).quad(128, 128, 116, 124, 4);
    p.fill(s, INK);
    s.n = 0;
    s.to(96, 126).quad(104, 122, 108, 130, 4).quad(100, 136, 94, 132, 4);
    p.fill(s, INK);
}

__attribute__((noinline)) void kuh_head(const Pen &h)
{
    Path q;
    q.to(84, 66).quad(78, 56, 72, 58, 4);
    h.stroke(q, 4, 0xE8D9B8);
    q.n = 0;
    q.to(116, 66).quad(122, 56, 128, 58, 4);
    h.stroke(q, 4, 0xE8D9B8);
    h.turn(-20, 76, 74).fill(Path::oval(76, 74, 9, 5, 18), HIDE, LINE, 1.4f);
    h.turn(20, 124, 74).fill(Path::oval(124, 74, 9, 5, 18), HIDE, LINE, 1.4f);
    h.ell(100, 80, 22, 20, HIDE, LINE, 1.8f);
    q.n = 0;
    q.to(88, 66).quad(96, 60, 100, 68, 4).quad(94, 74, 88, 66, 4);
    h.fill(q, INK);
    h.ell(100, 92, 16, 10, 0xF7B9C2, 0xE08C9A, 1.4f);
    h.ell(94, 92, 2, 2.6f, 0xC9707E);
    h.ell(106, 92, 2, 2.6f, 0xC9707E);
    for (float ex : {92.0f, 108.0f}) {
        h.ell(ex, 78, 2.6f, 3.2f, 0x2B2230);
        h.ell(ex + 0.9f, 76.8f, 0.9f, 0.9f, WHITE);
    }
    h.ell(86, 84, 3, 1.6f, mix(HIDE, 0xF49A94, 0.6f));
    h.ell(114, 84, 3, 1.6f, mix(HIDE, 0xF49A94, 0.6f));
}

__attribute__((noinline)) void kuh_bell(const Pen &bell)
{
    Path collar;
    collar.to(90, 100).quad(100, 106, 110, 100, 5);
    bell.stroke(collar, 3, 0xE5484D);
    bell.fill(poly({94, 104, 106, 104, 108, 118, 92, 118}), 0xF2C14E, 0xC99227, 1.4f);
    bell.ell(100, 119.5f, 2, 2, 0xC99227);
}

// A cow; her bell swings and rings, her head and tail move.
void o_kuh(const Ctx &x)
{
    const Pen &p = x.p;
    Path ground;
    ground.to(0, 158).quad(100, 146, 200, 158, 8).to(200, 200).to(0, 200);
    p.area(ground, 0xA8D08D);
    Path tail;
    tail.to(0, 0).quad(10, 10, 6, 26, 6);
    p.move(142, 108).turn(sinf(x.c * 2.5f) * 18).stroke(tail, 3.4f, HIDE);
    static const float LEGS[4][2] = {{70, 128}, {84, 132}, {118, 132}, {132, 128}};
    for (auto &l : LEGS) {
        p.rect(l[0] - 5, l[1], 10, 30, 3, HIDE, LINE, 1.4f);
        p.rect(l[0] - 5, l[1] + 24, 10, 6, 2, 0x5B4A44);
    }
    p.ell(100, 116, 44, 26, HIDE, LINE, 1.8f);
    kuh_spots(p);
    kuh_head(p.turn(sinf(x.c * 1.2f) * 3, 100, 96));
    kuh_bell(p.turn(sinf(x.c * 4) * 14, 100, 100));
    if (fabsf(sinf(x.c * 4)) > 0.85f) {
        for (int s = -1; s <= 1; s += 2) {
            p.line(100 + s * 14, 108, 100 + s * 19, 106, 1.8f, 0xC99227);
            p.line(100 + s * 15, 114, 100 + s * 21, 115, 1.8f, 0xC99227);
        }
    }
}

// A mountain lake; Knödel rows across, the peaks mirrored in the water.
void o_see(const Ctx &x)
{
    const Pen &p = x.p;
    constexpr uint32_t LAKE = 0x7FC4EA;
    scenery::peaks(p.move(100, 120).scale(0.82f, 0.55f), scenery::DAY);
    p.ell(100, 148, 96, 40, LAKE);
    const auto pale = [](uint32_t col) { return mix(LAKE, col, 0.25f); };
    const scenery::Palette mirrored = {pale(scenery::DAY.rock), pale(scenery::DAY.edge), pale(scenery::DAY.shade),
                                       pale(scenery::DAY.snow), pale(scenery::DAY.hill), pale(scenery::DAY.hill2)};
    scenery::peaks(p.move(100, 122).scale(0.82f, -0.3f), mirrored);
    static const float RIPPLES[3][2] = {{50, 138}, {150, 150}, {80, 172}};
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.4f + i / 3.0f);
        Path r;
        r.to(-10, 0).quad(0, -3, 10, 0, 4);
        p.move(RIPPLES[i][0], RIPPLES[i][1]).scale(1 + k).stroke(r, 1.6f, mix(LAKE, WHITE, 0.7f * (1 - k)));
    }
    const Pen boat = p.move(104 + sinf(x.c * 0.6f) * 18, 150 + sinf(x.c * 2) * 1.5f).turn(sinf(x.c * 1.7f) * 3);
    scenery::buddy(boat.move(0, -4).scale(0.78f), scenery::FLOAT, x.c, *x.st);
    Path hull;
    hull.to(-26, -6).to(26, -6).quad(22, 8, 0, 8, 5).quad(-22, 8, -26, -6, 5);
    boat.fill(hull, 0xC68B59, 0x8A5A3B, 1.8f);
    boat.line(-24, -2, 24, -2, 1.2f, 0x8A5A3B);
    boat.line(16, -8, 34, 10, 2.4f, 0x8A5A3B);
}

// A mountain hut: smoke from the chimney, geraniums at the windows.
void o_huette(const Ctx &x)
{
    const Pen &p = x.p;
    Path ground;
    ground.to(0, 156).quad(100, 146, 200, 156, 8).to(200, 200).to(0, 200);
    p.area(ground, 0xA8D08D);
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.35f + i / 3.0f), a = 0.85f * sinf(PI_F * k), r = 4 + k * 6;
        if (a > 0.05f) {
            p.ell(134 + sinf(k * 5) * 4 + k * 10, 36 - k * 34, r, r, WHITE, a);
        }
    }
    p.rect(128, 40, 12, 26, 0, 0x9AA4B1);
    p.rect(50, 92, 100, 64, 0, 0xC68B59, 0x8A5A3B, 2);
    for (int y = 100; y < 156; y += 9) {
        p.line(50, y, 150, y, 1.4f, 0xA8703F);
    }
    p.area(poly({36, 96, 100, 52, 164, 96, 156, 100, 100, 62, 44, 100}), 0x8A5A3B, 0x6E4528, 2);
    p.fill(poly({56, 96, 100, 66, 144, 96}), 0xE0B07E);
    p.rect(88, 116, 24, 40, 2, 0x8A5A3B, 0x6E4528, 1.6f);
    Path heart;
    heart.to(100, 128).cubic(98, 124, 93, 126, 95, 130, 4).to(100, 134).to(105, 130).cubic(107, 126, 102, 124, 100, 128, 4);
    p.fill(heart, 0xF3D2A6);
    static const float WINDOWS[2][2] = {{60, 108}, {120, 108}};
    for (int w = 0; w < 2; w++) {
        const float wx = WINDOWS[w][0], wy = WINDOWS[w][1];
        p.rect(wx, wy, 20, 18, 0, 0xDDF1FB, 0x6E4528, 1.8f);
        p.line(wx + 10, wy, wx + 10, wy + 18, 1.4f, 0x6E4528);
        p.rect(wx - 2, wy + 18, 24, 5, 1.5f, 0x8A5A3B);
        for (int i = 0; i < 3; i++) {
            const float r = 2.8f + sinf(x.c * 3 + w * 3 + i) * 0.35f;
            p.ell(wx + 3 + i * 7, wy + 17, r, r, 0xE5484D);
        }
    }
    p.rect(84, 76, 32, 11, 2.5f, 0xF3D2A6, 0x8A5A3B, 1.2f);
    x.text(p, "Hütte", 100, 81.5f, 7.5f, 0x6E4528);
}

// A cable car going up, Knödel in the cabin.
void o_seilbahn(const Ctx &x)
{
    const Pen &p = x.p;
    scenery::peaks(p.move(100, 176).scale(0.7f), scenery::DAY);
    p.fill(poly({150, 22, 160, 22, 166, 96, 144, 96}), 0x9AA4B1, 0x6B7180, 1.6f);
    p.line(140, 22, 170, 22, 3, 0x6B7180);
    p.line(-10, 104, 210, 14, 1.8f, INK);
    const float u = loop(x.c, 7.4f), cx = -20 + u * 240, cy = 108 - (cx + 10) * (90.0f / 220) + 26;
    const Pen cab = p.move(cx, cy).turn(sinf(x.c * 2) * 3, 0, -26);
    cab.line(0, -26, 0, -12, 2, INK);
    cab.ell(0, -27, 3, 3, INK);
    cab.rect(-22, -12, 44, 36, 7, 0xE5484D, 0xB8323A, 2);
    cab.rect(-17, -6, 34, 15, 3, 0xDDF1FB);
    scenery::buddy(cab.move(-1, 14).scale(0.62f), scenery::HAPPY, x.c, *x.st);
    cab.rect(-22, 9, 44, 15, 4, 0xE5484D);
    cab.rect(-22, 9, 44, 3, 0, mix(0xE5484D, WHITE, 0.6f));
}

// A valley with a village and its church; smoke from a chimney.
void o_tal(const Ctx &x)
{
    const Pen &p = x.p;
    p.area(poly({-10, 40, 40, 30, 100, 132, 160, 26, 210, 36, 210, 200, -10, 200}), 0xEEE7DD, 0xC9BBA8, 2);
    p.fill(poly({28, 34, 40, 30, 46, 46}), WHITE);
    p.fill(poly({154, 36, 160, 26, 170, 40}), WHITE);
    Path m1, m2, river;
    m1.to(-10, 96).quad(40, 92, 100, 150, 8).quad(160, 92, 210, 96, 8).to(210, 200).to(-10, 200);
    m2.to(-10, 150).quad(60, 140, 100, 160, 8).quad(140, 140, 210, 150, 8).to(210, 200).to(-10, 200);
    river.to(100, 156).quad(90, 172, 104, 186, 5).quad(114, 196, 108, 210, 5);
    p.area(m1, 0xA8D08D);
    p.area(m2, 0x8CC06E);
    p.stroke(river, 6, 0x7FC4EA);
    scenery::fir(p, 34, 112, 0.8f);
    scenery::fir(p, 166, 110, 0.8f, 0x4E9E68);
    scenery::fir(p, 52, 124, 0.6f);
    static const float HOUSES[4][3] = {{80, 158, 0xC2553A}, {122, 160, 0xC2553A}, {140, 150, 0xB8323A}, {66, 150, 0xB8323A}};
    for (auto &h : HOUSES) {
        p.rect(h[0] - 8, h[1] - 10, 16, 11, 0, WHITE, 0xC9CED6, 1);
        p.fill(poly({h[0] - 10, h[1] - 9, h[0], h[1] - 17, h[0] + 10, h[1] - 9}), (uint32_t)h[2]);
        p.rect(h[0] - 2, h[1] - 6, 4, 4, 0, 0x7FB6E0);
    }
    p.rect(96, 136, 12, 26, 0, WHITE, 0xC9CED6, 1);
    Path roof;
    roof.to(96, 136).quad(102, 120, 108, 136, 5);
    p.fill(roof, 0x5E9E45);
    p.ell(102, 128, 4, 5, 0x5E9E45);
    p.line(102, 123, 102, 115, 1.4f, 0xC99227);
    p.ell(102, 146, 2.6f, 2.6f, 0xF2C14E);
    for (int i = 0; i < 3; i++) {
        const float k = fract(x.c * 0.35f + i / 3.0f), a = 0.9f * sinf(PI_F * k), r = 2 + k * 4;
        if (a > 0.05f) {
            p.ell(142 + sinf(k * 6) * 2 + k * 6, 134 - k * 26, r, r, WHITE, a);
        }
    }
}

// ---- the objects ------------------------------------------------------------------

struct ObjDef {
    const char *id;
    void (*fn)(const Ctx &x);
    float dx, dy;  // small placement corrections (SHIFT in dinge.js)
};
const ObjDef OBJECTS[] = {
    {"sofa", o_sofa, 0, 0}, {"teppich", o_teppich, 0, 0}, {"lampe", o_lampe, 0, 14}, {"regal", o_regal, 0, 0},
    {"schrank", o_schrank, 0, 0}, {"spiegel", o_spiegel, 0, 0}, {"herd", o_herd, 0, 0}, {"heizung", o_heizung, 0, 0},
    {"steckdose", o_steckdose, 0, 0}, {"arzt", o_arzt, 0, -16}, {"praxis", o_praxis, 0, 0},
    {"krankenkasse", o_krankenkasse, 0, 0}, {"rezept", o_rezept, 0, 0}, {"apotheke", o_apotheke, 0, 0},
    {"medikament", o_medikament, 0, -16}, {"tablette", o_tablette, 0, 4},
    {"berg", o_berg, 0, 0}, {"wanderschuh", o_wanderschuh, 0, 0}, {"weg", o_weg, 0, 0}, {"wald", o_wald, 0, 0},
    {"wiese", o_wiese, 0, 0}, {"kuh", o_kuh, 0, 0}, {"see", o_see, 0, 0}, {"huette", o_huette, 0, 0},
    {"seilbahn", o_seilbahn, 0, 0}, {"tal", o_tal, 0, 0},
};
constexpr int OBJECT_COUNT = sizeof(OBJECTS) / sizeof(OBJECTS[0]);

int d_count(Kind kind) { return kind == PROP ? OBJECT_COUNT : 0; }
const char *d_name(Kind kind, int i) { return kind == PROP ? OBJECTS[i].id : ""; }
float d_pose_neutral(int) { return 0; }

void d_draw(G *g, const Stage &st, const Anim &anim, float, float clock)
{
    const float u = st.r / 100.0f;  // pixels per unit of the 200 x 200 disc
    const Pen disc = Pen::make(g, st.cx - 100 * u, st.cy - 100 * u, u);
    for (int i = 0; i < anim.n_prop; i++) {
        const ObjDef &o = OBJECTS[anim.prop[i]];
        o.fn({&st, disc.move(o.dx, o.dy), clock});
    }
}

}  // namespace

const Impl IMPL = {"dinge", d_count, d_name, d_pose_neutral, {{34, 44, 20}, {170, 156, 30}, {166, 30, 12}}, true, d_draw};

}  // namespace dinge
