#pragma once
// A small vector pen: shapes are given in the units of a drawing (the SVG
// coordinates of tools/preview/*.js) and land on the screen through an affine
// transform, so a prop can be moved, turned and scaled like an SVG group.

#include "gfx_util.h"

namespace vec {

using gfxu::G;

constexpr uint32_t NONE = 0xFF000000;  // "no fill" / "no stroke"

// An outline as a list of points; curves are flattened as they are added.
struct Path {
    static constexpr int MAX = 72;
    float p[MAX][2];
    int n = 0;

    Path &to(float x, float y)
    {
        if (n < MAX) {
            p[n][0] = x;
            p[n][1] = y;
            n++;
        }
        return *this;
    }
    Path &quad(float cx, float cy, float x, float y, int steps = 5)
    {
        const float x0 = p[n - 1][0], y0 = p[n - 1][1];
        for (int i = 1; i <= steps; i++) {
            const float u = (float)i / steps, v = 1 - u;
            to(v * v * x0 + 2 * v * u * cx + u * u * x, v * v * y0 + 2 * v * u * cy + u * u * y);
        }
        return *this;
    }
    Path &cubic(float ax, float ay, float bx, float by, float x, float y, int steps = 7)
    {
        const float x0 = p[n - 1][0], y0 = p[n - 1][1];
        for (int i = 1; i <= steps; i++) {
            const float u = (float)i / steps, v = 1 - u;
            to(v * v * v * x0 + 3 * v * v * u * ax + 3 * v * u * u * bx + u * u * u * x,
               v * v * v * y0 + 3 * v * v * u * ay + 3 * v * u * u * by + u * u * u * y);
        }
        return *this;
    }
    static Path rect(float x, float y, float w, float h, float r = 0)
    {
        Path q;
        if (r <= 0) {
            return q.to(x, y).to(x + w, y).to(x + w, y + h).to(x, y + h);
        }
        const float cx[4] = {x + w - r, x + w - r, x + r, x + r}, cy[4] = {y + r, y + h - r, y + h - r, y + r};
        for (int c = 0; c < 4; c++) {
            for (int i = 0; i <= 3; i++) {
                const float a = (c * 90 - 90 + i * 30) * 3.14159265f / 180.0f;
                q.to(cx[c] + cosf(a) * r, cy[c] + sinf(a) * r);
            }
        }
        return q;
    }
    static Path oval(float cx, float cy, float rx, float ry, int steps = 14)
    {
        Path q;
        for (int i = 0; i < steps; i++) {
            const float a = i * 2 * 3.14159265f / steps;
            q.to(cx + cosf(a) * rx, cy + sinf(a) * ry);
        }
        return q;
    }
};

struct Pen {
    G *g;
    float a, b, c, d, e, f;  // x' = a x + c y + e,  y' = b x + d y + f
    float k;                 // pixels per unit, for stroke widths and radii

    static Pen make(G *g, float ox, float oy, float k) { return {g, k, 0, 0, k, ox, oy, k}; }

    float X(float x, float y) const { return a * x + c * y + e; }
    float Y(float x, float y) const { return b * x + d * y + f; }

    // The same operations as an SVG transform list, read left to right.
    Pen move(float tx, float ty) const
    {
        Pen q = *this;
        q.e = X(tx, ty);
        q.f = Y(tx, ty);
        return q;
    }
    Pen turn(float degrees) const
    {
        const float r = degrees * 3.14159265f / 180.0f, cs = cosf(r), sn = sinf(r);
        Pen q = *this;
        q.a = a * cs + c * sn;
        q.b = b * cs + d * sn;
        q.c = -a * sn + c * cs;
        q.d = -b * sn + d * cs;
        return q;
    }
    Pen turn(float degrees, float cx, float cy) const { return move(cx, cy).turn(degrees).move(-cx, -cy); }
    Pen scale(float sx, float sy) const
    {
        Pen q = *this;
        q.a *= sx;
        q.b *= sx;
        q.c *= sy;
        q.d *= sy;
        q.k = k * sqrtf(fabsf(sx * sy));
        return q;
    }
    Pen scale(float s) const { return scale(s, s); }

    void line(float x0, float y0, float x1, float y1, float w, uint32_t col) const
    {
        gfxu::line(g, X(x0, y0), Y(x0, y0), X(x1, y1), Y(x1, y1), fmaxf(1.2f, w * k), col);
    }
    // Round-capped line whose radius goes from r0 to r1.
    void wedge(float x0, float y0, float x1, float y1, float r0, float r1, uint32_t col) const
    {
        gfxu::wedge(g, X(x0, y0), Y(x0, y0), X(x1, y1), Y(x1, y1), r0 * k, r1 * k, col);
    }
    // Ellipse with its axes along the screen's: for round things and small turns.
    void ell(float cx, float cy, float rx, float ry, uint32_t col, float alpha = 1) const
    {
        gfxu::ellipse(g, X(cx, cy), Y(cx, cy), rx * k, ry * k, col, alpha);
    }
    void ell(float cx, float cy, float rx, float ry, uint32_t fill, uint32_t stroke, float sw) const
    {
        ell(cx, cy, rx + sw / 2, ry + sw / 2, stroke);
        ell(cx, cy, rx - sw / 2, ry - sw / 2, fill);
    }
    void stroke(const Path &q, float w, uint32_t col, bool closed = false) const
    {
        for (int i = 0; i + 1 < q.n; i++) {
            line(q.p[i][0], q.p[i][1], q.p[i + 1][0], q.p[i + 1][1], w, col);
        }
        if (closed && q.n > 2) {
            line(q.p[q.n - 1][0], q.p[q.n - 1][1], q.p[0][0], q.p[0][1], w, col);
        }
    }
    // Fills a shape that is convex, or at least visible from its centre.
    void fill(const Path &q, uint32_t col, uint32_t edge = NONE, float sw = 0) const
    {
        if (q.n < 3) {
            return;
        }
        float mx = 0, my = 0;
        for (int i = 0; i < q.n; i++) {
            mx += q.p[i][0];
            my += q.p[i][1];
        }
        mx /= q.n;
        my /= q.n;
        const float cx = X(mx, my), cy = Y(mx, my);
        for (int i = 0; i < q.n; i++) {
            const int j = (i + 1) % q.n;
            g->fillTriangle(cx, cy, X(q.p[i][0], q.p[i][1]), Y(q.p[i][0], q.p[i][1]), X(q.p[j][0], q.p[j][1]),
                            Y(q.p[j][0], q.p[j][1]), col);
        }
        // The outline also smooths the edge of the triangles.
        if (edge != NONE && sw > 0) {
            stroke(q, sw, edge, true);
        } else {
            stroke(q, 0, col, true);
        }
    }
    // Fills any shape (concave too: a mountain range, a wave), row by row,
    // even-odd; `alpha` below 1 lets what is underneath show through (glass).
    void area(const Path &q, uint32_t col, uint32_t edge = NONE, float sw = 0, float alpha = 1) const
    {
        if (q.n < 3) {
            return;
        }
        float sx[Path::MAX], sy[Path::MAX], top = 1e9f, bottom = -1e9f;
        for (int i = 0; i < q.n; i++) {
            sx[i] = X(q.p[i][0], q.p[i][1]);
            sy[i] = Y(q.p[i][0], q.p[i][1]);
            top = fminf(top, sy[i]);
            bottom = fmaxf(bottom, sy[i]);
        }
        int32_t cx, cy, cw, ch;
        g->getClipRect(&cx, &cy, &cw, &ch);
        const int r0 = (int)fmaxf(cy, floorf(top)), r1 = (int)fminf(cy + ch - 1, ceilf(bottom));
        for (int row = r0; row <= r1; row++) {
            const float yc = row + 0.5f;
            float xs[Path::MAX];
            int n = 0;
            for (int i = 0; i < q.n; i++) {
                const int j = (i + 1) % q.n;
                if ((sy[i] <= yc && yc < sy[j]) || (sy[j] <= yc && yc < sy[i])) {
                    xs[n++] = sx[i] + (yc - sy[i]) / (sy[j] - sy[i]) * (sx[j] - sx[i]);
                }
            }
            for (int a = 1; a < n; a++) {  // insertion sort: a handful of crossings
                const float v = xs[a];
                int b = a - 1;
                for (; b >= 0 && xs[b] > v; b--) {
                    xs[b + 1] = xs[b];
                }
                xs[b + 1] = v;
            }
            for (int k = 0; k + 1 < n; k += 2) {
                const int x0 = (int)ceilf(xs[k] - 0.5f), x1 = (int)floorf(xs[k + 1] - 0.5f);
                if (x1 >= x0) {
                    gfxu::span(g, x0, row, x1 - x0 + 1, alpha, col);
                }
            }
        }
        if (alpha >= 0.98f || (edge != NONE && sw > 0)) {
            stroke(q, edge != NONE && sw > 0 ? sw : 0, edge != NONE && sw > 0 ? edge : col, true);
        }
    }
    void rect(float x, float y, float w, float h, float r, uint32_t col, uint32_t edge = NONE, float sw = 0) const
    {
        fill(Path::rect(x, y, w, h, r), col, edge, sw);
    }
    // Ellipse that turns with the pen.
    void oval(float cx, float cy, float rx, float ry, uint32_t col) const { fill(Path::oval(cx, cy, rx, ry), col); }
    void ring(float cx, float cy, float r, float w, uint32_t col) const { stroke(Path::oval(cx, cy, r, r, 24), w, col, true); }
    // Text does not turn or scale with the pen: only its anchor point moves.
    void text(const lgfx::IFont *font, const char *s, float x, float y, lgfx::textdatum_t datum, uint32_t col) const
    {
        if (font == nullptr) {
            return;
        }
        g->setFont(font);
        g->setTextDatum(datum);
        g->setTextColor(col);
        g->drawString(s, (int)lroundf(X(x, y)), (int)lroundf(Y(x, y)));
    }
};

}  // namespace vec
