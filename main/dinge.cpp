#include "dinge.h"

#include <initializer_list>

#include "gfx_util.h"
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
