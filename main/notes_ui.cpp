#include "notes_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "card_ui.h"
#include "gfx_util.h"
#include "scenery.h"
#include "ui_strings.h"

namespace notes_ui {

using G = lgfx::LovyanGFX;
using gfxu::mix;
using ui::CX;
using ui::CY;

// The prototype's colours, on the black of the AMOLED screen.
static constexpr uint32_t BLACK = 0x000000u, INK = 0xF5F2EAu, MUTED = 0x9A968Eu, DIM = 0x8A867Eu, FAINT = 0x5E5A53u,
                          LINE = 0x3A3833u, TEAL = 0x4FD1BFu, AMBER = 0xFFC24Au, RED = 0xFF5D52u, WARN = 0xFFB547u,
                          TRACK = 0x1E1E1Eu, TOAST = 0x171717u, TOAST_EDGE = 0x2C2C2Cu;
// The card's column: as wide as the circle allows from its top to its bottom.
static constexpr int X0 = 64, X1 = 404, TOP = 104, BODY = 150, BOTTOM = 392, PAGER_Y = 432;
static constexpr int ROW = 52, MAX_ROWS = 4;

static lgfx::PointerWrapper s_data;
static lgfx::VLWfont s_notes;
static const UiText *tx = &ui_text("en");

static bool s_loaded;

void init(const uint8_t *notes_font)
{
    if (notes_font != nullptr) {
        s_data.set(notes_font);
        s_loaded = s_notes.loadFont(&s_data);
    }
}

void set_locale(const char *locale) { tx = &ui_text(locale); }

// ---- text -------------------------------------------------------------------------------

static const lgfx::IFont *loc_s() { return ui::font(ui::FontId::LOC_S); }
static const lgfx::IFont *loc_m() { return ui::font(ui::FontId::LOC_M); }
// What was said; a pack without the notes font has only the locale's characters.
static const lgfx::IFont *nf() { return s_loaded ? (const lgfx::IFont *)&s_notes : loc_m(); }

static int width(G *g, const lgfx::IFont *f, const std::string &s)
{
    g->setFont(f);
    return g->textWidth(s.c_str());
}

static void text(G *g, const lgfx::IFont *f, const std::string &s, int x, int y, lgfx::textdatum_t datum, uint32_t col)
{
    g->setFont(f);
    g->setTextDatum(datum);
    g->setTextColor(col);
    g->drawString(s.c_str(), x, y);
}

// Length in bytes of the UTF-8 character at s[i], and its code point.
static int next(const std::string &s, size_t i, uint32_t &cp)
{
    const uint8_t c = (uint8_t)s[i];
    const int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    cp = n == 1 ? c : c & (0x3F >> (n - 1));
    for (int k = 1; k < n && i + k < s.size(); k++) {
        cp = cp << 6 | ((uint8_t)s[i + k] & 0x3F);
    }
    return n;
}

static std::string first_chars(const std::string &s, int count)
{
    if (count < 0) {
        return s;
    }
    size_t i = 0;
    uint32_t cp;
    for (int n = 0; i < s.size() && n < count; n++) {
        i += next(s, i, cp);
    }
    return s.substr(0, i);
}

// `s` shortened with "…" until it fits `max` pixels.
static std::string fit(G *g, const lgfx::IFont *f, std::string s, int max)
{
    if (width(g, f, s) <= max) {
        return s;
    }
    while (!s.empty()) {
        size_t i = s.size() - 1;
        while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) {
            i--;
        }
        s.erase(i);
        if (width(g, f, s + "…") <= max) {
            break;
        }
    }
    return s + "…";
}

// Lines of at most `max` pixels: Chinese breaks between any two characters,
// German and English between words. More than `lines` lines: the last ends in "…".
static std::vector<std::string> wrap(G *g, const lgfx::IFont *f, const std::string &s, int max, int lines)
{
    std::vector<std::string> tokens;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp;
        const int n = next(s, i, cp);
        const bool wide = cp >= 0x2E80, space = cp == ' ';
        // Punctuation that closes never starts a line: it stays with what comes before.
        static const char32_t CLOSING[] = U"，。、：；！？）」』》…,.;:!?)";
        bool closing = false;
        for (const char32_t *c = CLOSING; *c; c++) {
            closing |= cp == (uint32_t)*c;
        }
        if (closing && !tokens.empty()) {
            tokens.back().append(s, i, n);
            i += n;
            continue;
        }
        if (wide || space || tokens.empty()) {
            tokens.emplace_back(s, i, n);
        } else {
            uint32_t prev;
            const std::string &last = tokens.back();
            size_t j = last.size() - 1;
            while (j > 0 && ((uint8_t)last[j] & 0xC0) == 0x80) {
                j--;
            }
            next(last, j, prev);
            if (prev >= 0x2E80 || prev == ' ') {
                tokens.emplace_back(s, i, n);
            } else {
                tokens.back().append(s, i, n);
            }
        }
        i += n;
    }
    std::vector<std::string> out(1);
    for (const std::string &t : tokens) {
        if (!out.back().empty() && width(g, f, out.back() + t) > max) {
            if ((int)out.size() == lines) {
                out.back() = fit(g, f, out.back() + t, max);
                return out;
            }
            out.emplace_back(t == " " ? "" : t);
        } else if (!(out.back().empty() && t == " ")) {
            out.back() += t;
        }
    }
    return out;
}

// ---- shapes ----------------------------------------------------------------------------

static void pill(G *g, int x, int y, int w, int h, uint32_t fill, uint32_t edge = BLACK, float edge_w = 0)
{
    if (edge_w > 0) {
        g->fillSmoothRoundRect(x, y, w, h, h / 2, edge);
        g->fillSmoothRoundRect(x + edge_w, y + edge_w, w - 2 * edge_w, h - 2 * edge_w, h / 2 - edge_w, fill);
    } else {
        g->fillSmoothRoundRect(x, y, w, h, h / 2, fill);
    }
}

static void ring(G *g, float cx, float cy, float r, float w, uint32_t col, uint32_t inside = BLACK)
{
    g->fillSmoothCircle(cx, cy, r + w / 2, col);
    g->fillSmoothCircle(cx, cy, r - w / 2, inside);
}

// An arc of the circle around (cx, cy), clockwise from the top.
static void arc(G *g, float cx, float cy, float r, float from, float to, float w, uint32_t col)
{
    const int steps = (int)((to - from) * 120) + 1;
    for (int i = 0; i < steps; i++) {
        const float a0 = (from + (to - from) * i / steps) * 6.2831853f - 1.5707963f;
        const float a1 = (from + (to - from) * (i + 1) / steps) * 6.2831853f - 1.5707963f;
        gfxu::line(g, cx + cosf(a0) * r, cy + sinf(a0) * r, cx + cosf(a1) * r, cy + sinf(a1) * r, w, col);
    }
}

static void tick(G *g, float x, float y, float s, float w, uint32_t col)
{
    gfxu::line(g, x - 6 * s, y + 0.5f * s, x - 2 * s, y + 4.5f * s, w, col);
    gfxu::line(g, x - 2 * s, y + 4.5f * s, x + 6.5f * s, y - 5 * s, w, col);
}

static void cross(G *g, float x, float y, float s, float w, uint32_t col)
{
    gfxu::line(g, x - s, y - s, x + s, y + s, w, col);
    gfxu::line(g, x + s, y - s, x - s, y + s, w, col);
}

static void wifi(G *g, float x, float y, uint32_t col, bool off)
{
    for (int i = 0; i < 3; i++) {
        const float r = 4 + i * 4.5f;
        for (int k = 0; k < 6; k++) {
            const float a0 = (-135 + k * 15) * 0.0174533f, a1 = (-135 + (k + 1) * 15) * 0.0174533f;
            gfxu::line(g, x + cosf(a0) * r, y + 6 + sinf(a0) * r, x + cosf(a1) * r, y + 6 + sinf(a1) * r, 1.8f, col);
        }
    }
    g->fillSmoothCircle(x, y + 6, 1.8f, col);
    if (off) {
        gfxu::line(g, x - 9, y - 9, x + 9, y + 8, 1.8f, col);
    }
}

// The black circle dimmed over what was drawn (`alpha` of black).
static void dim(G *g, float alpha)
{
    for (int y = 0; y < ui::SCREEN; y++) {
        const float dy = y + 0.5f - CY, h = 234 * 234 - dy * dy;
        if (h > 0) {
            const int half = (int)sqrtf(h);
            gfxu::span(g, CX - half, y, 2 * half, alpha, BLACK);
        }
    }
}

static float ease_out(float u)
{
    u = gfxu::clampf(u);
    return 1 - (1 - u) * (1 - u) * (1 - u);
}

static float back_out(float u)
{
    u = gfxu::clampf(u);
    const float c = 1.6f;
    return 1 + (c + 1) * powf(u - 1, 3) + c * powf(u - 1, 2);
}

// ---- the notes page ---------------------------------------------------------------------

static struct {
    int rows;
    int row_y[MAX_ROWS], row_h[MAX_ROWS];
    bool card;
} layout;

static void chip(G *g, int x, int y, Kind kind)
{
    const uint32_t accent = kind == TODO ? TEAL : kind == IDEA ? AMBER : 0xB5B1A8u;
    const char *label = kind == TODO ? tx->todo : kind == IDEA ? tx->idea : tx->memo;
    const int w = width(g, loc_s(), label) + 50, h = 36;
    pill(g, x, y, w, h, kind == TODO || kind == IDEA ? mix(BLACK, accent, 0.16f) : 0x1A1A1Au);
    const float ix = x + 21, iy = y + h / 2.0f;
    if (kind == TODO) {
        g->fillSmoothRoundRect(ix - 8, iy - 8, 16, 16, 4, accent);
        g->fillSmoothRoundRect(ix - 6.3f, iy - 6.3f, 12.6f, 12.6f, 3, mix(BLACK, accent, 0.16f));
        tick(g, ix + 0.5f, iy - 0.5f, 0.62f, 1.7f, accent);
    } else if (kind == IDEA) {
        ring(g, ix, iy - 2.5f, 5.5f, 1.8f, accent, mix(BLACK, accent, 0.16f));
        gfxu::line(g, ix - 3, iy + 6, ix + 3, iy + 6, 1.8f, accent);
        gfxu::line(g, ix - 2, iy + 8.5f, ix + 2, iy + 8.5f, 1.8f, accent);
    } else {
        gfxu::wedge(g, ix, iy - 7, ix, iy + 1, 3.6f, 3.6f, accent);
        for (int k = 0; k < 6; k++) {
            const float a0 = k * 0.5236f, a1 = (k + 1) * 0.5236f;
            gfxu::line(g, ix + cosf(a0) * 6.5f, iy - 1 + sinf(a0) * 6.5f, ix + cosf(a1) * 6.5f, iy - 1 + sinf(a1) * 6.5f,
                       1.6f, accent);
        }
        gfxu::line(g, ix, iy + 5.5f, ix, iy + 8.5f, 1.6f, accent);
    }
    text(g, loc_s(), label, x + 36, y + h / 2, middle_left, accent);
}

static void status_line(G *g, const Status &s)
{
    char buf[48];
    const char *label = nullptr;
    uint32_t col = DIM;
    bool icon = true, off = false;
    if (s.no_key) {
        label = tx->no_key, col = WARN, icon = false;
    } else if (s.limited) {
        label = tx->limit_reached, col = WARN, icon = false;
    } else if (s.online) {
        label = tx->sending, col = TEAL;
    } else if (s.waiting > 0) {
        snprintf(buf, sizeof buf, tx->waiting_n, s.waiting);
        label = buf, off = true;
    }
    if (label == nullptr) {
        return;
    }
    // The icon spans y-7..y+8 around the y it is given: its middle on the text's.
    static constexpr int Y = 46;
    const int w = width(g, loc_s(), label) + (icon ? 28 : 0);
    if (icon) {
        wifi(g, CX - w / 2 + 9, Y, col, off);
    }
    text(g, loc_s(), label, CX - w / 2 + (icon ? 28 : 0), Y, middle_left, col);
}

static void pager(G *g, int index, int count)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%d / %d", index + 1, count);
    const lgfx::IFont *f = ui::font(ui::FontId::DE_S);
    const int w = width(g, f, buf);
    text(g, f, buf, CX, PAGER_Y, middle_center, DIM);
    const uint32_t prev = index > 0 ? 0xC9C5BCu : LINE, nxt = index < count - 1 ? 0xC9C5BCu : LINE;
    const float l = CX - w / 2.0f - 20, r = CX + w / 2.0f + 20;
    gfxu::line(g, l + 4, PAGER_Y - 7, l - 3, PAGER_Y, 2.2f, prev);
    gfxu::line(g, l - 3, PAGER_Y, l + 4, PAGER_Y + 7, 2.2f, prev);
    gfxu::line(g, r - 4, PAGER_Y - 7, r + 3, PAGER_Y, 2.2f, nxt);
    gfxu::line(g, r + 3, PAGER_Y, r - 4, PAGER_Y + 7, 2.2f, nxt);
}

// A task takes one row, or two lines when it is long; the rows stop where the
// card ends, the rest counted below.
static void todo_rows(G *g, const Card &c)
{
    int left = c.reveal;  // characters still to show, as the note comes in
    static constexpr int LINE_H = 30, MORE_H = 40;
    layout.rows = 0;
    int y = BODY;
    for (int i = 0; i < c.item_count && i < MAX_ROWS; i++) {
        const Item &it = c.items[i];
        const int tx0 = X0 + 42;
        const bool due = it.due != nullptr && it.due[0];
        const int dw = due ? width(g, loc_s(), it.due) + 22 : 0;
        const int right = due ? X1 - dw - 10 : X1;
        const auto lines = wrap(g, nf(), it.text, right - tx0, 2);
        const int h = lines.size() > 1 ? ROW + LINE_H : ROW;
        const bool last_fits = i == c.item_count - 1 ? y + h <= BOTTOM : y + h + MORE_H <= BOTTOM;
        if (!last_fits) {
            break;
        }
        layout.row_y[i] = y;
        layout.row_h[i] = h;
        layout.rows = i + 1;
        const int cy = y + ROW / 2;
        const float bx = X0 + 14;
        if (it.done) {
            g->fillSmoothCircle(bx, cy, 14, TEAL);
            tick(g, bx, cy, 1.0f, 2.6f, BLACK);
        } else {
            ring(g, bx, cy, 12.5f, 2.6f, 0x6E6A63u);
        }
        // As the note comes in, its characters appear line after line.
        int shown_total = 0;
        for (size_t k = 0; k < lines.size(); k++) {
            std::string line = lines[k];
            if (left >= 0) {
                const int n = (int)line.size();
                line = first_chars(line, left);
                left = left > n ? left - n : 0;
            }
            shown_total += (int)line.size();
            const int ly = cy + (int)k * LINE_H;
            text(g, nf(), line, tx0, ly, middle_left, it.done ? FAINT : INK);
            if (it.done && !line.empty()) {
                gfxu::line(g, tx0, ly + 1, tx0 + width(g, nf(), line), ly + 1, 2.0f, FAINT);
            }
        }
        if (due && (c.reveal < 0 || shown_total > 0)) {
            pill(g, X1 - dw, cy - 16, dw, 32, BLACK, LINE, 1.5f);
            text(g, loc_s(), it.due, X1 - dw / 2, cy, middle_center, it.done ? FAINT : 0xC9C5BCu);
        }
        y += h;
    }
    if (c.item_count > layout.rows) {
        char buf[24];
        snprintf(buf, sizeof buf, tx->more_n, c.item_count - layout.rows);
        text(g, loc_s(), buf, X0 + 42, y + 18, middle_left, DIM);
    }
}

void draw_page(G *g, const Card *c, int index, int count, const Status &status, float clock)
{
    g->fillScreen(BLACK);
    layout.rows = 0;
    layout.card = c != nullptr;
    status_line(g, status);
    if (c == nullptr) {
        // The empty page: a microphone and what to do.
        const float mx = CX, my = CY - 54;
        gfxu::wedge(g, mx, my - 16, mx, my + 8, 12, 12, 0x5E5A53u);
        gfxu::wedge(g, mx, my - 16, mx, my + 8, 9.5f, 9.5f, BLACK);
        for (int k = 0; k < 8; k++) {
            const float a0 = k * 0.3927f, a1 = (k + 1) * 0.3927f;
            gfxu::line(g, mx + cosf(a0) * 22, my + 2 + sinf(a0) * 22, mx + cosf(a1) * 22, my + 2 + sinf(a1) * 22, 2.4f,
                       0x5E5A53u);
        }
        gfxu::line(g, mx, my + 24, mx, my + 34, 2.4f, 0x5E5A53u);
        text(g, loc_m(), tx->no_notes, CX, CY + 14, middle_center, INK);
        text(g, loc_s(), tx->no_notes_hint, CX, CY + 54, middle_center, DIM);
        return;
    }
    chip(g, X0, TOP - 18, c->kind);
    const int cw = width(g, loc_s(), c->kind == TODO ? tx->todo : c->kind == IDEA ? tx->idea : tx->memo) + 50;
    text(g, loc_s(), c->when, X0 + cw + 12, TOP, middle_left, DIM);

    if (c->kind == TODO) {
        todo_rows(g, *c);
    } else if (c->kind == IDEA) {
        const std::string shown = first_chars(c->text, c->reveal);
        const auto lines = wrap(g, nf(), shown, X1 - X0, 6);
        for (size_t i = 0; i < lines.size(); i++) {
            text(g, nf(), lines[i], X0, BODY + 16 + (int)i * 38, middle_left, INK);
        }
    } else if (c->kind == WAITING) {
        for (int i = 0; i < 22; i++) {  // the recording, as a still wave
            const float h = 6 + fabsf(sinf(i * 0.9f + c->seconds) * sinf(i * 0.37f + c->seconds * 0.5f)) * 34;
            g->fillSmoothRoundRect(X0 + i * 10, BODY + 26 - h / 2, 6, h, 3, 0x4A4740u);
        }
        char buf[64];
        snprintf(buf, sizeof buf, "0:%02d · %s", (int)(c->seconds + 0.5f), status.online ? tx->sending : tx->waiting_wifi);
        text(g, loc_s(), buf, X0, BODY + 82, middle_left, MUTED);
    } else {  // SENDING: where the text will come, gently pulsing
        static const int W[3] = {300, 250, 170};
        for (int i = 0; i < 3; i++) {
            const float a = 0.16f + 0.2f * (0.5f + 0.5f * sinf(clock * 4.8f - i * 0.8f));
            g->fillSmoothRoundRect(X0, BODY + 6 + i * 36, W[i], 20, 10, mix(BLACK, INK, a));
        }
        text(g, loc_s(), tx->sending, X0, BODY + 136, middle_left, DIM);
    }
    if (count > 1) {
        pager(g, index, count);
    }
}

int item_at(int x, int y)
{
    if (!layout.card) {
        return -2;
    }
    for (int i = 0; i < layout.rows; i++) {
        if (y >= layout.row_y[i] && y < layout.row_y[i] + layout.row_h[i] && x >= X0 - 10 && x <= X1) {
            return i;
        }
    }
    return y >= TOP - 40 && y <= BOTTOM + 10 && x >= 30 && x <= ui::SCREEN - 30 ? -1 : -2;
}

// ---- the lists ----------------------------------------------------------------------------

static constexpr int LIST_TOP = 112, ROW_H = 70, TAB_Y = 82, TAB_H = 38;

static struct {
    int rows;
    int tab_x[2], tab_w[2];
} list_layout;

static void tabs(G *g, const ListPage &p)
{
    if (p.list == DONE) {  // one title: back is the back key
        char title[40];
        snprintf(title, sizeof title, "%s %d", tx->done_list, p.done);
        const int w = width(g, loc_s(), title) + 36;
        pill(g, CX - w / 2, TAB_Y - TAB_H / 2, w, TAB_H, mix(BLACK, TEAL, 0.18f));
        text(g, loc_s(), title, CX, TAB_Y, middle_center, TEAL);
        list_layout.tab_w[0] = list_layout.tab_w[1] = 0;
        return;
    }
    char label[2][32];
    snprintf(label[0], sizeof label[0], "%s %d", tx->todo, p.tasks);
    snprintf(label[1], sizeof label[1], "%s %d", tx->idea, p.ideas);
    const int w0 = width(g, loc_s(), label[0]) + 36, w1 = width(g, loc_s(), label[1]) + 36, gap = 10;
    const int x0 = CX - (w0 + w1 + gap) / 2;
    const int xs[2] = {x0, x0 + w0 + gap}, ws[2] = {w0, w1};
    const uint32_t accent[2] = {TEAL, AMBER};
    for (int i = 0; i < 2; i++) {
        const bool on = (int)p.list == i;
        pill(g, xs[i], TAB_Y - TAB_H / 2, ws[i], TAB_H, on ? mix(BLACK, accent[i], 0.18f) : BLACK, on ? accent[i] : LINE,
             on ? 0 : 1.5f);
        text(g, loc_s(), label[i], xs[i] + ws[i] / 2, TAB_Y, middle_center, on ? accent[i] : DIM);
        list_layout.tab_x[i] = xs[i];
        list_layout.tab_w[i] = ws[i];
    }
}

void draw_list(G *g, const ListPage &p, const Status &status, float clock)
{
    g->fillScreen(BLACK);
    status_line(g, status);
    tabs(g, p);
    list_layout.rows = p.row_count;
    layout.card = false;
    if (p.row_count == 0) {
        text(g, loc_m(), p.list == IDEAS ? tx->no_notes : tx->all_done, CX, CY + 20, middle_center, INK);
        text(g, loc_s(), tx->no_notes_hint, CX, CY + 60, middle_center, DIM);
        return;
    }
    for (int i = 0; i < p.row_count; i++) {
        const Row &r = p.rows[i];
        const int y = LIST_TOP + i * ROW_H, cy = y + ROW_H / 2;
        if (i == p.held) {
            g->fillSmoothRoundRect(X0 - 14, y + 3, X1 - X0 + 28, ROW_H - 6, 18, mix(BLACK, INK, 0.12f));
        }
        const int tx0 = p.list == IDEAS ? X0 + 18 : X0 + 42;
        const bool due = r.due != nullptr && r.due[0];
        const int dw = due ? width(g, loc_s(), r.due) + 22 : 0;
        const int right = due ? X1 - dw - 10 : X1;
        const std::string shown = first_chars(r.text, r.reveal);
        const auto lines = wrap(g, nf(), shown, right - tx0, 2);
        const int first = lines.size() > 1 ? cy - 14 : cy;
        if (p.list != IDEAS) {
            const float bx = X0 + 14;
            if (r.done) {
                g->fillSmoothCircle(bx, first, 14, TEAL);
                tick(g, bx, first, 1.0f, 2.6f, BLACK);
            } else {
                ring(g, bx, first, 12.5f, 2.6f, 0x6E6A63u, i == p.held ? mix(BLACK, INK, 0.12f) : BLACK);
            }
        } else {
            g->fillSmoothCircle(X0 + 4, first, 4, AMBER);
        }
        const bool struck = r.done && p.list == TASKS;  // ticked today, still among the tasks
        for (size_t k = 0; k < lines.size(); k++) {
            const int ly = first + (int)k * 28;
            text(g, nf(), lines[k], tx0, ly, middle_left, struck ? FAINT : p.list == DONE ? MUTED : INK);
            if (struck && !lines[k].empty()) {
                gfxu::line(g, tx0, ly + 1, tx0 + width(g, nf(), lines[k]), ly + 1, 2.0f, FAINT);
            }
        }
        if (due) {
            pill(g, X1 - dw, first - 16, dw, 32, i == p.held ? mix(BLACK, INK, 0.12f) : BLACK, LINE, 1.5f);
            text(g, loc_s(), r.due, X1 - dw / 2, first, middle_center, struck ? FAINT : 0xC9C5BCu);
        }
        if (i + 1 < p.row_count) {  // a hairline between rows
            g->drawFastHLine(tx0, y + ROW_H, X1 - tx0, mix(BLACK, LINE, 0.7f));
        }
    }
    if (p.pages > 1) {
        pager(g, p.page, p.pages);
    }
}

void draw_stats(G *g, const Stats &st, const Status &status)
{
    g->fillScreen(BLACK);
    status_line(g, status);
    ListPage p = {TASKS, st.tasks, st.ideas, 0, nullptr, 0, st.page, st.pages, -1};
    tabs(g, p);
    list_layout.rows = 0;
    int total = 0, most = 1;
    for (int n : st.week) {
        total += n;
        most = n > most ? n : most;
    }
    text(g, loc_s(), tx->week_done, CX, 146, middle_center, DIM);
    // The count, with its unit in the small text beside it.
    char buf[64];
    snprintf(buf, sizeof buf, "%d", total);
    const lgfx::IFont *digits = ui::font(ui::FontId::CLOCK);
    const int nw = width(g, digits, buf), uw = width(g, loc_s(), tx->tasks_unit);
    const int x = CX - (nw + 8 + uw) / 2;
    text(g, digits, buf, x, 206, middle_left, total ? INK : FAINT);
    text(g, loc_s(), tx->tasks_unit, x + nw + 8, 226, middle_left, DIM);
    // A bar a day, Monday to Sunday; today's in the accent, the days to come not yet there.
    static constexpr int BAR_W = 20, GAP = 16, BASE = 302, TALL = 50;
    const int x0 = CX - (7 * BAR_W + 6 * GAP) / 2;
    for (int d = 0; d < 7; d++) {
        const int bx = x0 + d * (BAR_W + GAP);
        const bool now = d == st.today, future = d > st.today;
        const int h = st.week[d] ? 8 + (TALL - 8) * st.week[d] / most : 4;
        const uint32_t col = future ? mix(BLACK, LINE, 0.6f) : now ? TEAL : st.week[d] ? mix(BLACK, TEAL, 0.45f) : LINE;
        g->fillSmoothRoundRect(bx, BASE - h, BAR_W, h, h < 10 ? h / 2 : 6, col);
        text(g, loc_s(), tx->week_initials[d], bx + BAR_W / 2, BASE + 22, middle_center, now ? TEAL : FAINT);
    }
    if (st.streak >= 2) {
        snprintf(buf, sizeof buf, tx->streak, st.streak);
        const int sw = width(g, loc_s(), buf);
        g->fillSmoothCircle(CX - sw / 2.0f - 12, 356, 4, AMBER);
        text(g, loc_s(), buf, CX + 6, 356, middle_center, AMBER);
    }
    // "See all", with a small chevron.
    const int aw = width(g, loc_s(), tx->see_all);
    text(g, loc_s(), tx->see_all, CX - 8, 392, middle_center, MUTED);
    const float cx = CX - 8 + aw / 2.0f + 12;
    gfxu::line(g, cx - 3, 386, cx + 3, 392, 2, MUTED);
    gfxu::line(g, cx + 3, 392, cx - 3, 398, 2, MUTED);
    if (st.pages > 1) {
        pager(g, st.page, st.pages);
    }
}

int row_at(int x, int y, int &tab)
{
    tab = -1;
    for (int i = 0; i < 2; i++) {
        if (y >= TAB_Y - TAB_H / 2 - 8 && y <= TAB_Y + TAB_H / 2 + 8 && x >= list_layout.tab_x[i] - 6 &&
            x <= list_layout.tab_x[i] + list_layout.tab_w[i] + 6) {
            tab = i;
            return -1;
        }
    }
    const int i = (y - LIST_TOP) / ROW_H;
    return y >= LIST_TOP && i < list_layout.rows && x >= 30 && x <= ui::SCREEN - 30 ? i : -1;
}

// ---- the menu ---------------------------------------------------------------------------

static void button_xy(bool three, int i, float &x, float &y)
{
    const int n = three ? 3 : 2;
    x = CX + (i - (n - 1) / 2.0f) * 110;
    y = CY - 6;
}

void draw_menu(G *g, Choice first, const char *title)
{
    const bool three = first == ALL_DONE || first == UNDO;
    dim(g, 0.86f);
    if (title != nullptr) {  // the row held, so that a wrong one is seen before anything happens
        text(g, nf(), fit(g, nf(), title, 300), CX, CY - 96, middle_center, INK);
    } else {
        text(g, loc_s(), tx->this_note, CX, CY - 92, middle_center, DIM);
    }
    const Choice choices[3] = {three ? first : NOTHING, REMOVE, CANCEL};
    for (int i = 0, b = three ? 0 : 1; b < 3; i++, b++) {
        float x, y;
        button_xy(three, i, x, y);
        const Choice c = choices[b];
        const uint32_t col = c == ALL_DONE || c == UNDO ? TEAL : c == REMOVE ? 0xFF6B5Eu : 0x4A4740u;
        ring(g, x, y, 41, 2.6f, col, c == CANCEL ? BLACK : mix(BLACK, col, 0.1f));
        if (c == ALL_DONE) {
            tick(g, x, y, 1.6f, 3.2f, TEAL);
        } else if (c == UNDO) {  // an arrow going back round
            arc(g, x, y + 2, 12, 0.08f, 0.78f, 2.8f, TEAL);
            gfxu::line(g, x - 12, y - 6, x - 13, y + 3, 2.8f, TEAL);
            gfxu::line(g, x - 12, y - 6, x - 4, y - 5, 2.8f, TEAL);
        } else if (c == REMOVE) {  // a bin
            gfxu::line(g, x - 13, y - 10, x + 13, y - 10, 2.6f, col);
            gfxu::line(g, x - 5, y - 15, x + 5, y - 15, 2.6f, col);
            gfxu::line(g, x - 10, y - 10, x - 8, y + 14, 2.6f, col);
            gfxu::line(g, x + 10, y - 10, x + 8, y + 14, 2.6f, col);
            gfxu::line(g, x - 8, y + 14, x + 8, y + 14, 2.6f, col);
            gfxu::line(g, x - 3, y - 4, x - 3, y + 8, 2.2f, col);
            gfxu::line(g, x + 3, y - 4, x + 3, y + 8, 2.2f, col);
        } else {
            cross(g, x, y, 9, 2.8f, 0xC9C5BCu);
        }
        text(g, loc_s(), c == ALL_DONE ? tx->all_done : c == UNDO ? tx->undo : c == REMOVE ? tx->remove : tx->cancel,
             (int)x, (int)y + 68, middle_center, 0xC9C5BCu);
    }
}

Choice menu_at(Choice first, int x, int y)
{
    const bool three = first == ALL_DONE || first == UNDO;
    const Choice choices[3] = {three ? first : NOTHING, REMOVE, CANCEL};
    for (int i = 0, b = three ? 0 : 1; b < 3; i++, b++) {
        float bx, by;
        button_xy(three, i, bx, by);
        if ((x - bx) * (x - bx) + (y - by) * (y - by) < 52 * 52) {
            return choices[b];
        }
    }
    return NOTHING;
}

// ---- recording ---------------------------------------------------------------------------

void draw_recording(G *g, float seconds, float max_seconds, float level, float clock)
{
    g->fillScreen(BLACK);
    const bool late = seconds >= max_seconds - 10;
    ring(g, CX, CY, 225, 4, TRACK);
    arc(g, CX, CY, 225, 0, gfxu::clampf(seconds / max_seconds), 4, late ? WARN : INK);
    // The sound, all around the edge.
    for (int i = 0; i < 96; i++) {
        const float a = i / 96.0f * 6.2831853f - 1.5707963f;
        const float amp = gfxu::clampf(level * 1.6f) * (0.55f + 0.45f * sinf(i * 0.55f + clock * 11)) *
                          (0.6f + 0.4f * sinf(i * 1.9f + clock * 5.9f));
        const float r0 = 186, r1 = r0 + 3 + fmaxf(0, amp) * 26;
        gfxu::line(g, CX + cosf(a) * r0, CY + sinf(a) * r0, CX + cosf(a) * r1, CY + sinf(a) * r1, 3,
                   mix(BLACK, RED, 0.3f + 0.7f * fmaxf(0, amp)));
    }
    char buf[16];
    snprintf(buf, sizeof buf, "%d:%02d", (int)seconds / 60, (int)seconds % 60);
    const lgfx::IFont *digits = ui::font(ui::FontId::CLOCK);
    const int w = width(g, digits, buf);
    text(g, digits, buf, CX + 12, CY - 20, middle_center, INK);
    if (fmodf(clock, 1.0f) < 0.6f) {
        g->fillSmoothCircle(CX + 12 - w / 2.0f - 24, CY - 22, 8, RED);
    }
    if (late) {
        snprintf(buf, sizeof buf, tx->seconds_left, (int)ceilf(max_seconds - seconds));
        text(g, loc_m(), buf, CX, CY + 52, middle_center, WARN);
    } else {
        text(g, loc_m(), tx->let_go_keep, CX, CY + 52, middle_center, INK);
    }
    text(g, loc_s(), tx->left_cancel, CX, CY + 92, middle_center, DIM);
}

void draw_message(G *g, Message m, float t)
{
    g->fillScreen(BLACK);
    const bool kept = m == KEPT_ONLINE || m == KEPT_LATER || m == KEPT_OFFLINE || m == KEPT_NO_KEY;
    if (kept && t < 0.5f) {  // the edge of the recording closes in
        const float u = ease_out(t * 2);
        ring(g, CX, CY, 218 * (1 - u) + 12, 4, mix(RED, BLACK, u));
    }
    const float s = back_out(t * 3.5f);
    const uint32_t col = kept ? TEAL : 0x4A4740u;
    const float by = CY - 40;
    if (s > 0.05f) {
        ring(g, CX, by, 37 * s, 3, col);
        if (kept) {
            tick(g, CX, by, 1.8f * s, 3.4f, TEAL);
        } else {
            cross(g, CX, by, 10 * s, 3, DIM);
        }
    }
    const char *title = kept ? tx->kept : m == TOO_SHORT ? tx->too_short : tx->cancelled;
    const char *sub = m == KEPT_ONLINE    ? tx->kept_online
                      : m == KEPT_LATER   ? tx->kept_later
                      : m == KEPT_OFFLINE ? tx->kept_offline
                      : m == KEPT_NO_KEY  ? tx->no_key
                      : m == TOO_SHORT    ? tx->too_short_hint
                                          : tx->cancelled_hint;
    const uint32_t fade = mix(BLACK, INK, gfxu::clampf(t * 4));
    text(g, loc_m(), title, CX, CY + 30, middle_center, fade);
    text(g, loc_s(), sub, CX, CY + 70, middle_center, m == KEPT_NO_KEY ? WARN : mix(BLACK, MUTED, gfxu::clampf(t * 4)));
}

// ---- the sentence book -------------------------------------------------------------------

// A face of a sentence as a column of lines; an empty font marks the short
// rule between the pattern and its topic.
struct Line {
    const lgfx::IFont *font;
    std::string text;
    int h;
    uint32_t col;
};

static void add_lines(std::vector<Line> &out, G *g, const lgfx::IFont *f, const std::string &s, int width, int h,
                      uint32_t col)
{
    for (std::string &l : wrap(g, f, s, width, 1000)) {
        out.push_back({f, std::move(l), h, col});
    }
}

void draw_sentence(G *g, const Sentence &s, int face, int index, int count, int &scroll)
{
    g->fillScreen(BLACK);
    static constexpr int WIDTH = 340, MID = CY - 6, AREA_TOP = 94;
    const int area_bottom = face < 2 ? 372 : 404;  // above the hint, or the pager
    const char *label = face == 0 ? tx->sentences : face == 1 ? tx->german : tx->pattern;
    text(g, loc_s(), label, CX, 66, middle_center, face == 2 ? AMBER : DIM);

    std::vector<Line> lines;
    if (face == 0) {
        add_lines(lines, g, nf(), s.meaning, WIDTH, 38, INK);
    } else if (face == 1) {
        // German in the cards' typeface; a long sentence in the smaller one.
        const lgfx::IFont *de = ui::font(ui::FontId::DE_M);
        if (wrap(g, de, s.german, WIDTH, 6).size() > 5) {
            add_lines(lines, g, nf(), s.german, WIDTH, 34, INK);
        } else {
            add_lines(lines, g, de, s.german, WIDTH, 40, INK);
        }
    } else {
        add_lines(lines, g, nf(), s.pattern[0] ? s.pattern : "–", WIDTH, 36, AMBER);
        if (s.topic[0]) {
            lines.push_back({nullptr, "", 24, LINE});
            add_lines(lines, g, nf(), s.topic, WIDTH - 40, 32, DIM);
        }
    }
    int h = 0;
    for (const Line &l : lines) {
        h += l.h;
    }

    // Centred when it fits; else from its top, scrolled, with a bar at the side.
    const int area = area_bottom - AREA_TOP;
    const bool scrolls = h > area;
    scroll = scrolls ? std::max(0, std::min(scroll, h - area)) : 0;
    int y = scrolls ? AREA_TOP - scroll : MID - h / 2;
    if (scrolls) {
        g->setClipRect(0, AREA_TOP, ui::SCREEN, area);
    }
    for (const Line &l : lines) {
        if (y + l.h > AREA_TOP - 40 && y < area_bottom + 40) {
            if (l.font) {
                text(g, l.font, l.text, CX, y + l.h / 2, middle_center, l.col);
            } else {
                g->drawFastHLine(CX - 30, y + l.h / 2, 60, l.col);
            }
        }
        y += l.h;
    }
    if (scrolls) {
        g->clearClipRect();
        // The text fades out where more of it lies beyond the edge.
        static constexpr int FADE = 26;
        for (int i = 0; i < FADE; i++) {
            const float alpha = 1 - (i + 0.5f) / FADE;
            if (scroll > 0) {
                gfxu::span(g, CX - 190, AREA_TOP + i, 380, alpha, BLACK);
            }
            if (scroll < h - area) {
                gfxu::span(g, CX - 190, area_bottom - 1 - i, 380, alpha, BLACK);
            }
        }
        static constexpr int BAR_X = CX + 192, BAR_TOP = 132, BAR_H = 204;
        const int thumb = std::max(28, BAR_H * area / h);
        const int at = BAR_TOP + (BAR_H - thumb) * scroll / (h - area);
        g->fillRoundRect(BAR_X - 1, BAR_TOP, 3, BAR_H, 1, LINE);
        g->fillRoundRect(BAR_X - 2, at, 5, thumb, 2, MUTED);
    }
    if (face < 2) {
        text(g, loc_s(), face == 0 ? tx->tap_german : tx->tap_pattern, CX, 396, middle_center, FAINT);
    }
    if (count > 1) {
        pager(g, index, count);
    }
}

void draw_no_sentences(G *g)
{
    g->fillScreen(BLACK);
    text(g, loc_m(), tx->no_sentences, CX, CY - 10, middle_center, INK);
    text(g, loc_s(), tx->add_on_site, CX, CY + 30, middle_center, DIM);
}

// ---- over any screen -----------------------------------------------------------------------

static constexpr int TOAST_Y = 360, TOAST_H = 46, TOAST_MAX_W = 340;

void toast_rect(int &x, int &y, int &w, int &h)
{
    x = CX - TOAST_MAX_W / 2;
    y = TOAST_Y;
    w = TOAST_MAX_W;
    h = TOAST_H;
}

void draw_toast(G *g, const char *s)
{
    const std::string shown = fit(g, nf(), s, TOAST_MAX_W - 70);
    const int w = width(g, nf(), shown) + 70;
    const int x = CX - w / 2;
    pill(g, x, TOAST_Y, w, TOAST_H, TOAST, TOAST_EDGE, 1.2f);
    tick(g, x + 26, TOAST_Y + TOAST_H / 2.0f, 1.0f, 2.4f, TEAL);
    text(g, nf(), shown, x + 46, TOAST_Y + TOAST_H / 2, middle_left, INK);
}

void draw_cheer(G *g, float t, float clock)
{
    const float up = back_out(t / 0.45f), down = ease_out((t - (CHEER_SECONDS - 0.4f)) / 0.4f);
    const float feet = 500 - 150 * up + 180 * (t > CHEER_SECONDS - 0.4f ? down : 0);
    dim(g, 0.55f * gfxu::clampf(t / 0.3f) * (1 - down));
    ui::draw_buddy(g, CX, feet, 118, scenery::LAUGH, clock);
    text(g, ui::font(ui::FontId::DE_MB), "Super!", CX, (int)feet - 150, middle_center, AMBER);
}

}  // namespace notes_ui
