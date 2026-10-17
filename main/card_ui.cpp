#include "card_ui.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "deck.h"
#include "gfx_util.h"
#include "character.h"
#include "vec.h"

using namespace gfxu;

namespace ui {

namespace {

// CONTEXT: the turn of a dialogue that does not hold the word being learned,
// between TEXT and the MUTED translation.
constexpr uint32_t BG = 0x000000, TEXT = 0xF2F4F7, CONTEXT = 0xB4BDCA, MUTED = 0x9AA6B6, RING_OFF = 0x2A313C, WHITE = 0xFFFFFF,
                   CHIP_TEXT = 0x10141A, DOT_OFF = 0x4A5361;
constexpr uint32_t COVER_COLOR = 0xC8862B, COVER_BG = 0xF6EAD3;
constexpr uint32_t KEY_A = 0xF2C14E, KEY_B = 0x4BA3D9;  // the yellow and the blue key

// Text stays inside this radius so nothing touches the progress ring, and
// above the face dots at the bottom.
constexpr float SAFE_R = 208.0f;
constexpr float DEG = 3.14159265f / 180.0f;
constexpr int DOTS_Y = 441, TEXT_BOTTOM = 428;

struct Font {
    lgfx::PointerWrapper data;
    lgfx::VLWfont font;
};
Font f_de_xl, f_de_l, f_de_m, f_de_mb, f_de_s, f_de_xs, f_loc_m, f_loc_s, f_loc_l, f_clock;
bool s_cjk;  // the locale is written without spaces

void load(Font &f, const uint8_t *blob)
{
    f.data.set(blob);
    f.font.loadFont(&f.data);
}

// On black the accent needs to be lighter than on the paper-white web cards.
uint32_t bright(uint32_t accent) { return mix(accent, WHITE, 0.42f); }

// Widest line that fits between y0 and y1 on the round screen.
float chord(float y0, float y1)
{
    float d = fmaxf(fabsf(y0 - CY), fabsf(y1 - CY));
    if (d >= SAFE_R || y1 > TEXT_BOTTOM) {
        return 0;
    }
    return 2.0f * sqrtf(SAFE_R * SAFE_R - d * d);
}

// One segment per item around the bezel, like the hour marks of a watch.
// The ring of the current screen, kept so that the animated stage, which is
// redrawn as a square, can put back the parts of the ring its corners cover.
struct Ring {
    int count, index;
    uint32_t accent;
    bool bold;
} s_ring = {0, 0, 0, false};

void draw_ring(G *g, int count, int index, uint32_t accent, bool bold = true)
{
    s_ring = {count, index, accent, bold};
    const float step = 360.0f / count, gap = count > 1 ? 5.0f : 0.0f;
    for (int i = 0; i < count; i++) {
        float a0 = -90.0f + i * step + gap / 2, a1 = -90.0f + (i + 1) * step - gap / 2;
        if (i == index) {
            g->fillArc(CX, CY, bold ? 233 : 231, bold ? 221 : 225, a0, a1, bright(accent));
        } else {
            g->fillArc(CX, CY, 231, 225, a0, a1, RING_OFF);
        }
    }
}

// Which face of the card is showing.
void draw_dots(G *g, int count, int index, uint32_t accent)
{
    const int step = 18, x0 = CX - (count - 1) * step / 2;
    for (int i = 0; i < count; i++) {
        if (i == index) {
            circle(g, x0 + i * step, DOTS_Y, 4.5f, bright(accent));
        } else {
            circle(g, x0 + i * step, DOTS_Y, 3.5f, DOT_OFF);
        }
    }
}

// ---- rich text --------------------------------------------------------------

struct Style {
    const lgfx::IFont *font;
    uint32_t color;
    bool operator!=(const Style &o) const { return font != o.font || color != o.color; }
};

// How one string is dressed: plain text, the card's keyword, and the fixed
// grammar colours for marked-up parts.
struct Styles {
    Style base;
    Style keyword;
    const lgfx::IFont *mark_font;  // font of prefix / reflexive / article / auxiliary
    const lgfx::IFont *cjk_font;   // font of CJK characters (when the locale has them)
};

struct Token {
    std::string text;
    Style style;
    bool space_before;
    bool glued;  // no line break before this token
    float width;
    bool break_before;  // a line break in the text ("\n", the turns of a dialogue)
};

struct Line {
    int first, last;  // token range [first, last)
    float width;
};

int utf8_len(unsigned char lead)
{
    return lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : 4;
}

uint32_t utf8_code(const char *p, int n)
{
    const unsigned char *u = (const unsigned char *)p;
    return n == 1 ? u[0]
         : n == 2 ? (u[0] & 0x1Fu) << 6 | (u[1] & 0x3Fu)
         : n == 3 ? (u[0] & 0x0Fu) << 12 | (u[1] & 0x3Fu) << 6 | (u[2] & 0x3Fu)
                  : (u[0] & 0x07u) << 18 | (u[1] & 0x3Fu) << 12 | (u[2] & 0x3Fu) << 6 | (u[3] & 0x3Fu);
}

// Characters drawn with the CJK font and breakable one by one. Curly quotes,
// dashes and the ellipsis count too, so they match the text around them.
bool is_cjk(uint32_t cp) { return cp >= 0x2E80 || (cp >= 0x2014 && cp <= 0x2026 && cp != 0x2022); }

// Punctuation that must not start a line.
bool is_closing(uint32_t cp)
{
    static const uint32_t MARKS[] = {0xFF0C, 0x3002, 0xFF01, 0xFF1F, 0x3001, 0xFF09, 0xFF1B, 0xFF1A, 0x201D, 0x2019, 0x2026};
    for (uint32_t m : MARKS) {
        if (cp == m) {
            return true;
        }
    }
    return false;
}

Style style_for(const std::string &role, const Styles &st)
{
    const Theme &th = deck_theme();
    if (role.empty()) {
        return st.keyword;
    }
    if (role == "prefix") {
        return {st.mark_font, th.prefix};
    }
    if (role == "refl") {
        return {st.mark_font, th.refl};
    }
    if (role == "der" || role == "die" || role == "das") {
        return {st.mark_font, th.gender[role == "der" ? 0 : role == "die" ? 1 : 2]};
    }
    if (role == "haben" || role == "sein") {
        return {st.mark_font, th.aux[role == "sein"]};
    }
    return st.base;  // unknown roles render as plain text
}

// Splits a string with {{markup}} into tokens: words for German and English,
// single characters for CJK.
std::vector<Token> tokenize(const char *s, const Styles &st)
{
    std::vector<Token> out;
    std::string cur;
    Style cur_style = st.base;
    bool space = false, glue = false, brk = false;

    auto flush = [&]() {
        if (!cur.empty()) {
            // The separator of a grammar line ("fuhr · ist gefahren") never starts a line.
            const bool separator = cur == "·" && !out.empty() && !brk;
            out.push_back({cur, cur_style, space, (glue && !space && !brk) || separator, 0, brk});
            cur.clear();
            space = false;
            brk = false;
        }
        glue = false;
    };
    auto feed = [&](const char *p, const char *end, const Style &style) {
        while (p < end) {
            const int n = utf8_len((unsigned char)*p);
            const uint32_t cp = utf8_code(p, n);
            if (cp == ' ') {
                flush();
                space = true;
            } else if (cp == '\n') {
                flush();
                brk = !out.empty();
                space = false;
            } else if (s_cjk && is_cjk(cp)) {
                const bool attached = !cur.empty();
                flush();
                const Style cs = {st.cjk_font, style.color};
                if (is_closing(cp) && !out.empty() && !space && !brk && !(out.back().style != cs)) {
                    out.back().text.append(p, n);
                } else {
                    out.push_back({std::string(p, n), cs, space, is_closing(cp) && (attached || !out.empty()) && !space && !brk, 0, brk});
                }
                space = false;
                brk = false;
            } else {
                if (!cur.empty() && cur_style != style) {
                    flush();
                    glue = true;  // a style change inside a word
                }
                if (cur.empty()) {
                    cur_style = style;
                }
                cur.append(p, n);
            }
            p += n;
        }
    };

    const char *p = s;
    while (*p) {
        const char *open = strstr(p, "{{");
        if (open == nullptr) {
            feed(p, p + strlen(p), st.base);
            break;
        }
        feed(p, open, st.base);
        const char *close = strstr(open, "}}");
        if (close == nullptr) {
            feed(open, open + strlen(open), st.base);
            break;
        }
        const char *bar = close;
        while (bar > open + 2 && bar[-1] >= 'a' && bar[-1] <= 'z') {
            bar--;
        }
        const bool has_role = bar > open + 2 && bar < close && bar[-1] == '|';
        const std::string role = has_role ? std::string(bar, close) : std::string();
        feed(open + 2, has_role ? bar - 1 : close, style_for(role, st));
        p = close + 2;
    }
    flush();
    return out;
}

void measure(G *g, std::vector<Token> &tokens)
{
    for (auto &tk : tokens) {
        g->setFont(tk.style.font);
        tk.width = g->textWidth(tk.text.c_str());
    }
}

float total_width(const std::vector<Token> &tokens, float space)
{
    float w = 0;
    for (size_t i = 0; i < tokens.size(); i++) {
        w += tokens[i].width + (i > 0 && tokens[i].space_before ? space : 0);
    }
    return w;
}

template <typename Limit>
std::vector<Line> wrap(const std::vector<Token> &tokens, float space, Limit limit)
{
    std::vector<Line> lines;
    Line cur = {0, 0, 0};
    const int n = (int)tokens.size();
    for (int i = 0; i < n;) {
        // A group is a token plus everything glued to it.
        int end = i + 1;
        float group = tokens[i].width;
        while (end < n && tokens[end].glued) {
            group += tokens[end].width + (tokens[end].space_before ? space : 0);
            end++;
        }
        float lead = (cur.last > cur.first && tokens[i].space_before) ? space : 0;
        if (cur.last > cur.first && (tokens[i].break_before || cur.width + lead + group > limit((int)lines.size()))) {
            lines.push_back(cur);
            cur = {i, i, 0};
            lead = 0;
        }
        cur.width += lead + group;
        cur.last = end;
        i = end;
    }
    if (cur.last > cur.first) {
        lines.push_back(cur);
    }
    return lines;
}

// Wraps into as few lines as the limits allow, then narrows the limits as far
// as possible without adding a line, so the lines come out evenly long.
template <typename Limit>
std::vector<Line> wrap_balanced_part(const std::vector<Token> &tokens, float space, Limit limit);

// Balances each part between line breaks on its own: one long turn of a
// dialogue should not make the other turn break unevenly.
template <typename Limit>
std::vector<Line> wrap_balanced(const std::vector<Token> &tokens, float space, Limit limit)
{
    std::vector<Line> lines;
    const int n = (int)tokens.size();
    for (int a = 0; a < n;) {
        int b = a + 1;
        while (b < n && !tokens[b].break_before) {
            b++;
        }
        const std::vector<Token> part(tokens.begin() + a, tokens.begin() + b);
        const int first_line = (int)lines.size();
        for (Line ln : wrap_balanced_part(part, space, [&](int i) { return limit(first_line + i); })) {
            ln.first += a;
            ln.last += a;
            lines.push_back(ln);
        }
        a = b;
    }
    return lines;
}

template <typename Limit>
std::vector<Line> wrap_balanced_part(const std::vector<Token> &tokens, float space, Limit limit)
{
    std::vector<Line> best = wrap(tokens, space, limit);
    float lo = 0.4f, hi = 1.0f;
    for (int i = 0; i < 7; i++) {
        float mid = (lo + hi) / 2;
        auto trial = wrap(tokens, space, [&](int line) { return limit(line) * mid; });
        if (trial.size() <= best.size()) {
            best = trial;
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return best;
}

// Does the token end a phrase? (CJK comma, colon, full stop, equals sign ...)
bool ends_phrase(const Token &tk)
{
    static const char *const MARKS[] = {"，", "；", "：", "！", "？", "。", "、", "＝"};
    for (auto m : MARKS) {
        const size_t n = strlen(m);
        if (tk.text.size() >= n && tk.text.compare(tk.text.size() - n, n, m) == 0) {
            return true;
        }
    }
    return false;
}

// Chinese has no spaces, so even wrapping may cut a word in two. Lines that
// end at punctuation never do: use them when they need no extra line.
template <typename Limit>
std::vector<Line> wrap_phrases(const std::vector<Token> &tokens, float space, Limit limit)
{
    const std::vector<Line> even = wrap_balanced(tokens, space, limit);
    std::vector<Line> lines;
    Line cur = {0, 0, 0}, pending = {0, 0, 0};  // pending: tokens since the last phrase end
    const int n = (int)tokens.size();
    for (int i = 0; i < n; i++) {
        if (tokens[i].break_before && (cur.last > cur.first || pending.last > pending.first)) {
            // A line break in the text: what came before ends its line here.
            if (pending.last > pending.first) {
                cur.width += pending.width;
                cur.last = pending.last;
            }
            lines.push_back(cur);
            cur = {i, i, 0};
            pending = {i, i, 0};
        }
        const bool first = pending.last == pending.first && cur.last == cur.first;
        pending.width += tokens[i].width + (!first && tokens[i].space_before ? space : 0);
        pending.last = i + 1;
        const bool glued_next = i + 1 < n && tokens[i + 1].glued;
        if ((ends_phrase(tokens[i]) && !glued_next) || i + 1 == n) {
            if (cur.last > cur.first && cur.width + pending.width > limit((int)lines.size())) {
                lines.push_back(cur);
                // The phrase starts the next line: drop the space in front of it.
                const float lead = tokens[pending.first].space_before ? space : 0;
                cur = {pending.first, pending.last, pending.width - lead};
            } else {
                cur.width += pending.width;
                cur.last = pending.last;
            }
            pending = {i + 1, i + 1, 0};
        }
    }
    if (cur.last > cur.first) {
        lines.push_back(cur);
    }
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].width > limit(i)) {
            return even;
        }
    }
    return lines.size() <= even.size() ? lines : even;
}

// ---- a column of items, centred on the round screen ------------------------------

struct Item {
    enum Type { TEXT, PILL } type;
    int gap_before;
    // TEXT
    std::vector<Token> tokens;
    float space;
    int line_h, baseline;
    bool phrases;
    std::vector<Line> lines;
    int n;  // lines reserved
    // PILL
    std::string pill_text;
    Kasus kasus;
    uint32_t accent;
    // result
    int y, height;
};

constexpr int PILL_H = 50, CHIP_H = 32;
const char *const KASUS_LABEL[] = {"", "+ Akk", "+ Dat", "+ Gen"};

Item text_item(G *g, const char *s, const Styles &st, int line_h, int baseline, int gap_before, bool phrases = false)
{
    Item it = {};
    it.type = Item::TEXT;
    it.gap_before = gap_before;
    it.tokens = tokenize(s, st);
    measure(g, it.tokens);
    g->setFont(st.base.font);
    it.space = g->textWidth(" ");
    it.line_h = line_h;
    it.baseline = baseline;
    it.phrases = phrases;
    it.n = 1;
    return it;
}

Item pill_item(const char *text, Kasus kasus, uint32_t accent, int gap_before)
{
    Item it = {};
    it.type = Item::PILL;
    it.gap_before = gap_before;
    it.pill_text = text;
    it.kasus = kasus;
    it.accent = accent;
    return it;
}

int pill_width(G *g, const Item &it, int *text_w = nullptr, int *chip_w = nullptr)
{
    g->setFont(&f_de_mb.font);
    const int tw = g->textWidth(it.pill_text.c_str());
    g->setFont(&f_de_s.font);
    const int cw = it.kasus ? g->textWidth(KASUS_LABEL[it.kasus]) + 20 : 0;
    if (text_w) {
        *text_w = tw;
    }
    if (chip_w) {
        *chip_w = cw;
    }
    return 24 + tw + (cw ? 12 + cw + 10 : 24);
}

// Places the items for the line counts in `n` and wraps the text. Sets `grown`
// if some text needs more lines than it was given.
bool place(G *g, std::vector<Item> &items, int shift, bool &grown)
{
    int total = 0;
    for (auto &it : items) {
        it.height = it.type == Item::PILL ? PILL_H : it.n * it.line_h;
        total += it.gap_before + it.height;
    }
    int y = CY - 6 + shift - total / 2;
    bool fits = true;
    grown = false;
    for (auto &it : items) {
        y += it.gap_before;
        it.y = y;
        y += it.height;
        if (it.type == Item::PILL) {
            // The ends of a pill are round, so its corners need less room.
            fits = fits && pill_width(g, it) <= chord(it.y + 12, it.y + PILL_H - 12);
            continue;
        }
        auto limit = [&](int i) { return chord(it.y + i * it.line_h, it.y + (i + 1) * it.line_h); };
        it.lines = it.phrases ? wrap_phrases(it.tokens, it.space, limit) : wrap_balanced(it.tokens, it.space, limit);
        grown = grown || (int)it.lines.size() > it.n;
        for (int i = 0; i < (int)it.lines.size(); i++) {
            fits = fits && it.lines[i].width <= limit(i);
        }
    }
    return fits;
}

// Finds line counts so that everything fits the circle, for a block moved
// down by `shift` pixels from the centre.
bool layout_at(G *g, std::vector<Item> &items, int shift)
{
    for (auto &it : items) {
        it.n = 1;
    }
    bool grown = true, fits = false;
    for (int round = 0; round < 12 && grown; round++) {
        fits = place(g, items, shift, grown);
        for (auto &it : items) {
            if (it.type == Item::TEXT && (int)it.lines.size() > it.n) {
                it.n = (int)it.lines.size();
            }
        }
    }
    if (grown) {
        return false;
    }
    // Lines reserved on the way but not needed in the end: give them back if that still works.
    std::vector<Item> tight = items;
    for (auto &it : tight) {
        if (it.type == Item::TEXT) {
            it.n = (int)it.lines.size();
        }
    }
    bool tight_grown;
    if (place(g, tight, shift, tight_grown) && !tight_grown) {
        items = tight;
        return true;
    }
    return fits;
}

// Centred if possible. A block that is wide at one end (a pill on top, a long
// last line) may only fit a little lower or higher. Returns false if nothing fits.
bool layout(G *g, std::vector<Item> &items)
{
    static const int SHIFTS[] = {0, 10, -10, 20, -20, 30};
    for (int shift : SHIFTS) {
        if (layout_at(g, items, shift)) {
            return true;
        }
    }
    layout_at(g, items, 0);
    return false;
}

void draw_tokens(G *g, const std::vector<Token> &tokens, const Line &ln, float space, int baseline_y)
{
    g->setTextDatum(baseline_left);
    float x = CX - ln.width / 2;
    for (int i = ln.first; i < ln.last; i++) {
        const Token &tk = tokens[i];
        if (i > ln.first && tk.space_before) {
            x += space;
        }
        g->setFont(tk.style.font);
        g->setTextColor(tk.style.color);
        g->drawString(tk.text.c_str(), (int)lroundf(x), baseline_y);
        x += tk.width;
    }
}

void render(G *g, const std::vector<Item> &items)
{
    for (const auto &it : items) {
        if (it.type == Item::TEXT) {
            for (int i = 0; i < (int)it.lines.size(); i++) {
                draw_tokens(g, it.tokens, it.lines[i], it.space, it.y + i * it.line_h + it.baseline);
            }
            continue;
        }
        // Pattern pill, with the governed case as a chip.
        int text_w, chip_w;
        const int w = pill_width(g, it, &text_w, &chip_w), x = CX - w / 2;
        g->fillSmoothRoundRect(x, it.y, w, PILL_H, PILL_H / 2, mix(BG, it.accent, 0.34f));
        g->setFont(&f_de_mb.font);
        g->setTextDatum(middle_left);
        g->setTextColor(bright(it.accent));
        g->drawString(it.pill_text.c_str(), x + 24, it.y + PILL_H / 2 + 1);
        if (chip_w) {
            const int chip_x = x + 24 + text_w + 12, chip_y = it.y + (PILL_H - CHIP_H) / 2;
            g->fillSmoothRoundRect(chip_x, chip_y, chip_w, CHIP_H, CHIP_H / 2, deck_theme().kasus[it.kasus - 1]);
            g->setFont(&f_de_s.font);
            g->setTextDatum(middle_center);
            g->setTextColor(CHIP_TEXT);
            g->drawString(KASUS_LABEL[it.kasus], chip_x + chip_w / 2, chip_y + CHIP_H / 2 + 1);
        }
    }
}

// One centred line. Tries each font in turn and uses the first that fits.
// `mark`: colour of {{marked}} parts (0: the same as the rest).
bool draw_single(G *g, const char *s, uint32_t color, bool marked, const Font *const *fonts, int n_fonts, int y,
                 uint32_t mark = 0)
{
    for (int i = 0; i < n_fonts; i++) {
        const lgfx::IFont *f = &fonts[i]->font;
        const Styles st = {{f, color}, {f, mark ? mark : color}, f, f};
        std::vector<Token> tokens;
        if (marked) {
            tokens = tokenize(s, st);
        } else {
            tokens.push_back({s, st.base, false, false, 0, false});
        }
        measure(g, tokens);
        g->setFont(f);
        const float space = g->textWidth(" "), w = total_width(tokens, space);
        const float half = g->fontHeight() * 0.34f;  // about the height of the letters
        const bool fits = w <= chord(y - half, y + half);
        if (fits || i + 1 == n_fonts) {
            // Vertically centred on y: the baseline sits a little below the middle.
            draw_tokens(g, tokens, {0, (int)tokens.size(), w}, space, (int)lroundf(y + g->fontHeight() * 0.27f));
            return fits;
        }
    }
    return false;
}

// Three lines under the stage. Face 1: lead, headword, gloss. Cover: the
// title in the learner's language, the German title, level and size.
bool draw_title_block(G *g, const char *lead, const char *head, bool marked, const char *gloss, uint32_t accent, bool cover)
{
    static const Font *const HEAD[] = {&f_de_xl, &f_de_l, &f_de_mb};
    static const Font *const SMALL[] = {&f_de_s};
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    const bool has_lead = lead[0] != 0;
    bool fits = true;
    if (has_lead) {
        fits = draw_single(g, lead, MUTED, false, cover ? LOCAL : SMALL, cover ? 2 : 1, 328) && fits;
    }
    fits = draw_single(g, head, bright(accent), marked, HEAD, 3, has_lead ? 365 : 350) && fits;
    if (gloss[0]) {
        fits = draw_single(g, gloss, MUTED, false, cover ? SMALL : LOCAL, cover ? 1 : 2, has_lead ? 409 : 401) && fits;
    }
    return fits;
}

Styles german(const Card &card, const lgfx::IFont *regular)
{
    return {{regular, TEXT}, {&f_de_mb.font, bright(card.color)}, &f_de_mb.font, &f_loc_m.font};
}

// A deck's title in the learner's language, its German title, level and size,
// as one block centred below the picture. The German title is always drawn in
// the same font; one that does not fit one line is split into two lines of
// about equal width.
bool draw_cover(G *g, int deck)
{
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    static const Font *const TITLES[] = {&f_de_l, &f_de_mb};
    static const Font *const META[] = {&f_de_s};
    constexpr int CENTRE_Y = 352, LOCAL_GAP = 40, LINE_GAP = 44, META_GAP = 38;

    const DeckInfo info = deck_info(deck);
    g->fillScreen(BG);
    draw_ring(g, deck_count(), deck, COVER_COLOR, false);
    const std::string meta = std::string(info.level) + " · " + std::to_string(info.cards) + " Karten";

    // The title in the large font, on one line or split into two; if a line
    // is still too wide (one long word: "Fitnessstudio"), all of it in the smaller one.
    std::string lines[2];
    const Font *const *TITLE = TITLES;
    for (const Font *const &font : TITLES) {
        TITLE = &font;
        g->setFont(&font->font);
        const float half = g->fontHeight() * 0.34f;
        const int one_line_y = CENTRE_Y - (LOCAL_GAP + META_GAP) / 2 + LOCAL_GAP;
        lines[0] = info.title;
        lines[1] = "";
        if (g->textWidth(info.title) > chord(one_line_y - half, one_line_y + half)) {
            // Split at the space that makes the longer line shortest.
            const std::string t = info.title;
            int best = -1;
            for (size_t i = t.find(' '); i != std::string::npos; i = t.find(' ', i + 1)) {
                const int w = std::max(g->textWidth(t.substr(0, i).c_str()), g->textWidth(t.substr(i + 1).c_str()));
                if (best < 0 || w < best) {
                    best = w;
                    lines[0] = t.substr(0, i);
                    lines[1] = t.substr(i + 1);
                }
            }
        }
        // Each line on its own row of the two-line layout.
        const int top = CENTRE_Y - (LOCAL_GAP + LINE_GAP + META_GAP) / 2 + LOCAL_GAP;
        bool fits = true;
        for (int i = 0; i < (lines[1].empty() ? 1 : 2); i++) {
            const int y = lines[1].empty() ? one_line_y : top + i * LINE_GAP;
            fits = fits && g->textWidth(lines[i].c_str()) <= chord(y - half, y + half);
        }
        if (fits) {
            break;
        }
    }
    const int n = lines[1].empty() ? 1 : 2;
    const int height = LOCAL_GAP + (n - 1) * LINE_GAP + META_GAP;
    int y = CENTRE_Y - height / 2;
    bool fits = draw_single(g, info.title_local, MUTED, false, LOCAL, 2, y);
    y += LOCAL_GAP;
    for (int i = 0; i < n; i++, y += LINE_GAP) {
        fits = draw_single(g, lines[i].c_str(), bright(COVER_COLOR), false, TITLE, 1, y) && fits;
    }
    return draw_single(g, meta.c_str(), MUTED, false, META, 1, y - LINE_GAP + META_GAP) && fits;
}

bool draw_front(G *g, int deck, int index, const Card &card)
{
    g->fillScreen(BG);
    draw_ring(g, deck_info(deck).cards, index, card.color);
    draw_dots(g, face_count(deck, index), 0, card.color);
    return draw_title_block(g, card.lead, card.headword, true, card.gloss, card.color, false);
}

// In a dialogue, the turns without a marked word are only context: one step
// dimmer, so the eye goes to the turn with the word.
void dim_context_turns(std::vector<Token> &tokens, const Style &base)
{
    const int n = (int)tokens.size();
    int turns = 0;
    for (int i = 0; i < n; i++) {
        turns += i == 0 || tokens[i].break_before;
    }
    if (turns < 2) {
        return;
    }
    for (int a = 0; a < n;) {
        int b = a + 1;
        while (b < n && !tokens[b].break_before) {
            b++;
        }
        bool marked = false;
        for (int i = a; i < b; i++) {
            marked = marked || tokens[i].style != base;
        }
        for (int i = a; i < b && !marked; i++) {
            tokens[i].style.color = CONTEXT;
        }
        a = b;
    }
}

bool draw_example(G *g, int deck, int index, const Card &card)
{
    g->fillScreen(BG);
    draw_ring(g, deck_info(deck).cards, index, card.color);
    draw_dots(g, face_count(deck, index), 1, card.color);

    std::vector<Item> items;
    if (card.grammar_kind == GRAMMAR_PATTERN) {
        items.push_back(pill_item(card.grammar, card.kasus2, card.color, 0));
    } else if (card.grammar_kind != GRAMMAR_NONE) {
        items.push_back(text_item(g, card.grammar, german(card, &f_de_mb.font), 37, 27, 0));
    }
    const Styles de = german(card, &f_de_m.font);
    items.push_back(text_item(g, card.example, de, 37, 27, items.empty() ? 0 : 20));
    dim_context_turns(items.back().tokens, de.base);
    const Styles tr = {{&f_loc_s.font, MUTED}, {&f_loc_s.font, MUTED}, &f_loc_s.font, &f_loc_s.font};
    items.push_back(text_item(g, card.translation, tr, s_cjk ? 31 : 29, 23, 16, s_cjk));
    const bool fits = layout(g, items);
    render(g, items);
    return fits;
}

bool draw_tip(G *g, int deck, int index, const Card &card)
{
    g->fillScreen(BG);
    draw_ring(g, deck_info(deck).cards, index, card.color);
    draw_dots(g, face_count(deck, index), 2, card.color);

    std::vector<Item> items;
    const Styles label = {{&f_de_s.font, MUTED}, {&f_de_s.font, MUTED}, &f_de_s.font, &f_de_s.font};
    items.push_back(text_item(g, "T I P P", label, 28, 21, 0));
    if (card.pattern3[0]) {
        items.push_back(pill_item(card.pattern3, card.kasus3, card.color, 12));
    }
    if (card.tip[0]) {
        items.push_back(text_item(g, card.tip, german(card, &f_de_m.font), 40, 29, 16, s_cjk));
    }
    const bool fits = layout(g, items);
    render(g, items);
    return fits;
}

int s_stage_card_deck = -1, s_stage_card_index = -1;  // card cached by draw_stage

void restore_ring(G *g)
{
    if (s_ring.count > 0) {
        draw_ring(g, s_ring.count, s_ring.index, s_ring.accent, s_ring.bold);
    }
}

int s_stage_x = STAGE_X, s_stage_y = STAGE_Y, s_stage_size = STAGE_SIZE;  // set by stage_disc
// A stage with the fonts every character needs.
character::Stage stage_at(int cx, int cy, int r, uint32_t bg, uint32_t accent)
{
    return {cx, cy, r, bg, accent, &f_de_xs.font, &f_de_s.font, &f_de_xl.font};
}

// The empty disc is the same every frame: draw it once per colour and copy it.
void stage_disc(G *g, const character::Stage &at, int who)
{
    static lgfx::LGFX_Sprite disc;
    static uint32_t disc_bg = 0xFFFFFFFF;
    static int disc_who = -1, disc_r = -1;
    const int size = at.r * 2 + 3;
    if (disc.getBuffer() == nullptr || disc.width() != size) {
        disc.deleteSprite();
        disc.setPsram(true);
        disc.setColorDepth(16);
        disc.createSprite(size, size);
        disc_r = -1;
    }
    if (disc_bg != at.bg || disc_who != who || disc_r != at.r) {
        character::Stage st = at;
        st.cx = st.cy = at.r + 1;
        disc.fillScreen(BG);
        character::draw_background(&disc, st, who);
        disc_bg = at.bg;
        disc_who = who;
        disc_r = at.r;
    }
    disc.pushSprite(g, at.cx - at.r - 1, at.cy - at.r - 1);
    s_stage_x = at.cx - at.r - 1;
    s_stage_y = at.cy - at.r - 1;
    s_stage_size = size;
}

// The deck's character greeting on the cover. Names a character does not have
// are ignored, so one list serves them all: Knödel raises an arm, Brezel a paw.
const character::Anim &cover_anim(int who)
{
    static character::Anim a;
    static int ready = -1;
    if (ready != who) {
        character::anim_reset(a, who);
        character::anim_set_pose(a, "happy", 1);
        character::anim_set_pose(a, "smile", 0.9f);
        character::anim_set_pose(a, "blush", 0.4f);
        character::anim_set_pose(a, "browY", -2);
        character::anim_set_pose(a, "armL", 30);
        character::anim_set_pose(a, "armR", 150);
        character::anim_set_pose(a, "hRx", 44);
        character::anim_set_pose(a, "hRy", -76);
        character::anim_add_motion(a, "hop");
        character::anim_add_motion(a, "tail-wag");
        character::anim_add_fx(a, "sparkles");
        ready = who;
    }
    return a;
}

int s_hour, s_minute, s_second;  // set_clock()

float fract(float v) { return v - floorf(v); }

// Backgrounds of the drawn menu pictures, inside a disc already drawn by stage_disc.
void disc_vgradient(G *g, float cx, float cy, float r, uint32_t top, uint32_t bottom)
{
    for (int y = (int)ceilf(cy - r + 1); y <= (int)floorf(cy + r - 1); y++) {
        const float dy = y + 0.5f - cy, hw = sqrtf(fmaxf(0, (r - 1.2f) * (r - 1.2f) - dy * dy));
        if (hw > 0.5f) {
            g->drawFastHLine((int)lroundf(cx - hw), y, (int)lroundf(2 * hw), mix(top, bottom, (y - (cy - r)) / (2 * r)));
        }
    }
}

void disc_rgradient(G *g, float cx, float cy, float r, uint32_t centre, uint32_t edge)
{
    const int steps = 14;
    for (int i = 0; i < steps; i++) {
        const float u = (float)i / (steps - 1);
        circle(g, cx, cy, (r - 1.2f) * (1 - u * 0.92f), mix(edge, centre, u));
    }
}

// A four-pointed twinkle.
void twinkle(G *g, float x, float y, float size, uint32_t color)
{
    if (size < 0.8f) {
        return;
    }
    wedge(g, x, y, x + size, y, size * 0.3f, 0.3f, color);
    wedge(g, x, y, x - size, y, size * 0.3f, 0.3f, color);
    wedge(g, x, y, x, y + size, size * 0.3f, 0.3f, color);
    wedge(g, x, y, x, y - size, size * 0.3f, 0.3f, color);
}

// A gear with a drop shadow, bevelled teeth and a hub.
void gear(G *g, float cx, float cy, float r, int teeth, float angle, float k, uint32_t body, uint32_t dark, uint32_t light)
{
    const float root = r - 9 * k, outer = r;
    vec::Path p;
    for (int i = 0; i < teeth; i++) {
        const float a = angle + i * 2 * 3.14159265f / teeth, half = 3.14159265f / teeth;
        const float tw = half * 0.42f, bw = half * 0.62f;
        p.to(cosf(a - bw) * root, sinf(a - bw) * root);
        p.to(cosf(a - tw) * outer, sinf(a - tw) * outer);
        p.to(cosf(a + tw) * outer, sinf(a + tw) * outer);
        p.to(cosf(a + bw) * root, sinf(a + bw) * root);
    }
    const vec::Pen shadow = vec::Pen::make(g, cx + 3 * k, cy + 5 * k, 1);
    shadow.fill(p, 0x000000 | mix(0xC9D2DF, dark, 0.35f));
    const vec::Pen pen = vec::Pen::make(g, cx, cy, 1);
    pen.fill(p, body, dark, 1.6f * k);
    // Bevel: a lighter ring inside the teeth, then the hub.
    circle(g, cx, cy, root - 4 * k, light);
    circle(g, cx, cy, root - 8 * k, body);
    circle(g, cx, cy, r * 0.32f + 2 * k, dark);
    circle(g, cx, cy, r * 0.32f, light);
    circle(g, cx, cy, r * 0.15f, dark);
    // A soft highlight on the upper left.
    ellipse(g, cx - r * 0.38f, cy - r * 0.42f, r * 0.16f, r * 0.08f, mix(light, 0xFFFFFF, 0.5f), 0.7f);
}

// Knödel's hold on the clock's string: its right arm raised, body leaning back.
constexpr float HOLD_ARM = 158, HOLD_ROT = -12;

// Pictures of the main menu that are not characters: an analog clock showing
// the time, and a tomato for the focus timer.
void menu_icon(G *g, const character::Stage &st, MenuIcon icon, float clock)
{
    const float k = st.r / 150.0f, cx = st.cx, cy = st.cy;
    if (icon == ICON_CLOCK) {
        // Time slipping away: a clock with wings tugs to get off, Knödel holds
        // it back. The clock shows the real time.
        character::Stage face = st;
        face.bg = 0xE7EEF6;
        stage_disc(g, face, 0);
        const uint32_t INK = 0x2E3A4F;
        const float ground = cy + 92 * k, R0 = st.r;
        // The clock tugs in surges: 0 resting, 1 at the top of a surge.
        const float surge = powf(0.5f + 0.5f * sinf(clock * 1.9f), 3);
        // Sky fading to a warm horizon, two clouds drifting by, a meadow.
        disc_vgradient(g, cx, cy, R0, 0xCFE3F7, 0xFBEFD9);
        for (int i = 0; i < 2; i++) {
            // Each cloud drifts across the part of the disc that is wide enough for it.
            const float y = cy - (i ? 86 : 104) * k, sc = (i ? 0.8f : 1.0f) * k;
            const float hw = sqrtf(fmaxf(0, (R0 - 4 * k) * (R0 - 4 * k) - (y - cy) * (y - cy))) - 36 * sc;
            const float x = cx - hw + fmodf(clock * (6 + 4 * i) * k + i * 90 * k, 2 * hw);
            ellipse(g, x, y, 22 * sc, 9 * sc, 0xFFFFFF, 0.9f);
            ellipse(g, x - 12 * sc, y + 2 * sc, 13 * sc, 7 * sc, 0xFFFFFF, 0.9f);
            ellipse(g, x + 6 * sc, y - 6 * sc, 13 * sc, 9 * sc, 0xFFFFFF, 0.9f);
        }
        for (int y = (int)ground; y < (int)(cy + R0); y++) {
            const float dy = y + 0.5f - cy, hw = sqrtf(fmaxf(0, (R0 - 1.2f) * (R0 - 1.2f) - dy * dy));
            g->drawFastHLine((int)lroundf(cx - hw), y, (int)lroundf(2 * hw), mix(0xBFDDA0, 0x9CC67A, (y - ground) / (R0 * 0.6f)));
        }
        {
            const float hw = sqrtf(fmaxf(0, (R0 - 1.2f) * (R0 - 1.2f) - (ground - cy) * (ground - cy)));
            g->drawFastHLine((int)lroundf(cx - hw), (int)ground, (int)lroundf(2 * hw), 0xD9EDC4u);
        }
        // Time flies: the clock has wings and pulls away to the upper right.
        const float dx = cx + (40 + surge * 8) * k, dy = cy - (26 + surge * 10) * k, R = 52 * k;
        const float tilt = (8 + surge * 6) * DEG, flap = sinf(clock * 11) * 24;
        ellipse(g, dx - 10 * k, ground + 1 * k, R * (0.6f - surge * 0.15f), 4 * k, mix(0xBFDDA0, 0x000000, 0.1f));
        for (int side = -1; side <= 1; side += 2) {
            // A wing: four feathers fanning up and out from the rim of the clock.
            const vec::Pen w = vec::Pen::make(g, dx + side * R * 0.8f, dy - R * 0.45f, k).turn(side * (-28 + flap));
            for (int f = 0; f < 4; f++) {
                const float len = 46 - f * 8, a = side * (f * 16 - 14);
                w.turn(a).fill(vec::Path::oval(side * len * 0.5f, 0, len * 0.5f, 8 - f * 1.2f), 0xFFFFFF, 0xC9D3DE, 1.2f);
            }
        }
        circle(g, dx, dy, R + 4 * k, INK);
        circle(g, dx, dy, R, 0xFFFFFF);
        for (int i = 0; i < 12; i++) {
            const float a = i * 30 * DEG + tilt, r0 = (i % 3 ? 51 : 45) * k;
            line(g, dx + sinf(a) * r0, dy - cosf(a) * r0, dx + sinf(a) * 56 * k, dy - cosf(a) * 56 * k, (i % 3 ? 2.2f : 4) * k, INK);
        }
        const float hours = ((s_hour % 12) + s_minute / 60.0f) * 30 * DEG + tilt;
        const float minutes = (s_minute + s_second / 60.0f) * 6 * DEG + tilt;
        const float seconds = (s_second + fmodf(clock, 1.0f)) * 6 * DEG + tilt;
        line(g, dx, dy, dx + sinf(hours) * 29 * k, dy - cosf(hours) * 29 * k, 7 * k, INK);
        line(g, dx, dy, dx + sinf(minutes) * 43 * k, dy - cosf(minutes) * 43 * k, 5 * k, INK);
        line(g, dx, dy, dx + sinf(seconds) * 48 * k, dy - cosf(seconds) * 48 * k, 2 * k, 0xE5484D);
        circle(g, dx, dy, 4.5f * k, 0xE5484D);

        // Knödel holds on with all it has: leaning back, heels in the grass,
        // pulled forward and off its feet a little at every surge.
        static character::Anim holder;
        static bool ready = false;
        if (!ready) {
            character::anim_reset(holder, 0);
            static const char *const NAMES[] = {"armR", "armL", "rot", "browY", "browT", "open", "mopen", "smile", "lookX", "lookY"};
            static const float VALUES[] = {HOLD_ARM, 110, HOLD_ROT, -3, -0.4f, 1.2f, 0.55f, -0.2f, 2.4f, -2.2f};
            for (int i = 0; i < 10; i++) {
                character::anim_set_pose(holder, NAMES[i], VALUES[i]);
            }
            character::anim_add_motion(holder, "tremble");
            character::anim_add_fx(holder, "sweat");
            ready = true;
        }
        const float unit = 0.92f * k;  // pixels per unit of Knödel's own 200 x 200 stage
        const float feet_x = cx - 70 * k + surge * 10 * k, lift = surge * surge * 8 * k;
        // Dust kicked up behind the heels as they slide.
        for (int i = 0; i < 3; i++) {
            const float u = fract(clock * 1.9f / 6.2832f + i / 3.0f);
            const float px = feet_x - (18 + u * 26) * k, py = ground - (3 + u * 10 + i * 2) * k, rr = (3 + u * 5) * k;
            if (surge > 0.1f) {
                circle(g, px, py, rr, mix(0xE8D9B8, 0xBFDDA0, u), (1 - u) * 0.9f);
            }
        }
        character::Stage ks = st;
        ks.r = (int)(100 * unit);
        ks.cx = (int)(feet_x + 10 * unit);
        ks.cy = (int)(ground - 66 * unit - lift);
        // The string, from the raised paw to the bottom of the clock, taut. The
        // paw: Knödel's right arm (18 units from the shoulder at 31, -36),
        // scaled and turned with the body about the feet.
        const float arm = HOLD_ARM * DEG, ks_scale = 1.3f * unit, rot = HOLD_ROT * DEG;
        const float hx = 31 + 18 * sinf(arm), hy = -36 + 18 * cosf(arm);
        const float px = feet_x + (hx * cosf(rot) - hy * sinf(rot)) * ks_scale;
        const float py = ground - lift + (hx * sinf(rot) + hy * cosf(rot)) * ks_scale;
        const float tx = dx - R * 0.5f, ty = dy + R * 0.87f;
        quad(g, px, py, (px + tx) / 2 + 3 * k, (py + ty) / 2 + 4 * k * (1 - surge), tx, ty, 2.2f * k, INK);
        character::draw(g, ks, holder, 3.0f, clock);
        return;
    }
    // A tomato that breathes a little, with a dial of the minutes going round it.
    character::Stage face = st;
    face.bg = 0xFBE3DC;
    stage_disc(g, face, 0);
    disc_rgradient(g, cx, cy, st.r, 0xFFF6F2, 0xF8D6CC);
    const float pulse = 1 + 0.02f * sinf(clock * 2.2f), r = 74 * k * pulse, ty = cy + 10 * k;
    const float sweep = fmodf(clock * 24, 360);
    g->fillArc(cx, cy, (int)(112 * k), (int)(104 * k), -90, -90 + 360, 0xF6D3C9u);
    g->fillArc(cx, cy, (int)(112 * k), (int)(104 * k), -90, -90 + sweep, 0xE5484Du);
    ellipse(g, cx, ty, r * 1.08f, r, 0xE5484D);
    ellipse(g, cx - r * 0.38f, ty - r * 0.42f, r * 0.26f, r * 0.15f, 0xF59A8F);
    for (int i = 0; i < 5; i++) {
        const float a = (i * 72 - 90) * DEG + sinf(clock * 1.5f) * 0.08f;
        wedge(g, cx, ty - r * 0.9f, cx + cosf(a) * 30 * k, ty - r * 0.9f + sinf(a) * 14 * k, 7 * k, 2 * k, 0x5E9E45);
    }
    line(g, cx, ty - r * 0.9f, cx + 6 * k, ty - r * 1.2f, 6 * k, 0x3E7A2F);
}

// Six ways to cheer: who, how they move, and on which colour.
const character::Anim &celebration_anim(int variant, uint32_t &bg, uint32_t &accent)
{
    struct Spec {
        const char *who;
        const char *motions[3];
        const char *fx[2];
        const char *pose[6];
        float values[6];
        uint32_t bg, accent;
    };
    static const Spec SPECS[CELEBRATIONS] = {
        {"knoedel", {"jump-wave"}, {"confetti"}, {"happy", "smile", "blush", "armL", "armR"}, {1, 0.95f, 0.5f, 150, 150}, 0xFFF1D6, 0xE0A526},
        {"knoedel", {"spin"}, {"sparkles", "star-orbit"}, {"happy", "smile", "armL", "armR"}, {1, 0.9f, 100, 100}, 0xE6F0FF, 0x4B7BD9},
        {"knoedel", {"hop"}, {"music-notes"}, {"happy", "smile", "blush", "armL", "armR"}, {0.8f, 0.8f, 0.6f, 60, 130}, 0xFDE7F0, 0xD9487B},
        {"katze", {"dance", "tail-wag"}, {"hearts"}, {"happy", "smile", "hLx", "hLy", "hRx", "hRy"}, {1, 0.9f, -30, -70, 30, -70}, 0xFFE9E3, 0xE5484D},
        {"katze", {"bounce", "tail-wag"}, {"sparkles"}, {"happy", "smile", "hLx", "hLy", "hRx", "hRy"}, {1, 0.95f, -48, -62, 48, -62}, 0xEAF6E4, 0x3E8E2F},
        {"katze", {"hop", "tail-wag"}, {"sun", "sparkles"}, {"smile", "blush", "hRx", "hRy", "earL", "earR"}, {0.9f, 0.5f, 44, -84, 8, 8}, 0xFFF4D6, 0xE0A526},
    };
    static character::Anim anims[CELEBRATIONS];
    static bool ready[CELEBRATIONS];
    const Spec &sp = SPECS[variant];
    if (!ready[variant]) {
        const int who = character::find(sp.who);
        character::Anim &a = anims[variant];
        character::anim_reset(a, who < 0 ? 0 : who);
        for (int i = 0; i < 6 && sp.pose[i]; i++) {
            character::anim_set_pose(a, sp.pose[i], sp.values[i]);
        }
        for (int i = 0; i < 3 && sp.motions[i]; i++) {
            character::anim_add_motion(a, sp.motions[i]);
        }
        for (int i = 0; i < 2 && sp.fx[i]; i++) {
            character::anim_add_fx(a, sp.fx[i]);
        }
        ready[variant] = true;
    }
    bg = sp.bg;
    accent = sp.accent;
    return anims[variant];
}

// The focus timer's companions: Brezel writing (or waiting while paused),
// Knödel humming during the break.
const character::Anim &timer_anim(bool focus, bool running)
{
    static character::Anim anims[3];
    static bool ready[3];
    const int i = !focus ? 2 : running ? 0 : 1;
    if (!ready[i]) {
        character::Anim &a = anims[i];
        const int cat = character::find("katze");
        character::anim_reset(a, i == 2 || cat < 0 ? 0 : cat);
        if (i == 0) {
            character::anim_add_prop(a, "clipboard");
            character::anim_add_prop(a, "pen");
            character::anim_add_motion(a, "write");
            character::anim_add_motion(a, "tail-wag");
            character::anim_set_pose(a, "lookY", 1.4f);
            character::anim_set_pose(a, "smile", 0.4f);
        } else if (i == 1) {
            character::anim_add_motion(a, "breathe");
            character::anim_add_motion(a, "tail-wag");
            character::anim_set_pose(a, "lid", 0.3f);
        } else {
            character::anim_add_motion(a, "sway-slow");
            character::anim_add_motion(a, "breathe");
            character::anim_add_fx(a, "music-notes");
            character::anim_set_pose(a, "happy", 0.7f);
            character::anim_set_pose(a, "smile", 0.8f);
        }
        ready[i] = true;
    }
    return anims[i];
}

}  // namespace

void init(const FontBlobs &b)
{
    load(f_de_xl, b.de_xl);
    load(f_de_l, b.de_l);
    load(f_de_m, b.de_m);
    load(f_de_mb, b.de_mb);
    load(f_de_s, b.de_s);
    load(f_de_xs, b.de_xs);
    load(f_clock, b.clock);
}

void set_locale(const uint8_t *loc_m, const uint8_t *loc_s, const uint8_t *loc_l)
{
    f_loc_m.font.unloadFont();
    f_loc_s.font.unloadFont();
    f_loc_l.font.unloadFont();
    load(f_loc_m, loc_m);
    load(f_loc_s, loc_s);
    load(f_loc_l, loc_l);
    const char *loc = deck_locale();
    s_cjk = strncmp(loc, "zh", 2) == 0 || strncmp(loc, "ja", 2) == 0;
    s_stage_card_deck = -1;  // cached strings belong to the previous pack
}

bool draw_menu(G *g, int index, int count, const char *title)
{
    static const Font *const TITLE[] = {&f_loc_l, &f_loc_m};
    g->fillScreen(BG);
    draw_ring(g, count, index, COVER_COLOR, false);
    return draw_single(g, title, bright(COVER_COLOR), false, TITLE, 2, MENU_TITLE_Y);
}

void draw_menu_stage(G *g, MenuIcon icon, float t, float clock)
{
    character::Stage st = stage_at(CX, MENU_CY, MENU_R, icon == ICON_GEAR ? 0xE3E7EEu : COVER_BG, COVER_COLOR);
    if (icon == ICON_CLOCK || icon == ICON_TIMER) {
        menu_icon(g, st, icon, clock);
        restore_ring(g);
        return;
    }
    if (icon != ICON_GEAR) {
        const int who = icon == ICON_BREZEL ? character::find("katze") : 0;
        stage_disc(g, st, who < 0 ? 0 : who);
        character::draw(g, st, cover_anim(who < 0 ? 0 : who), t, clock);
        restore_ring(g);
        return;
    }
    stage_disc(g, st, 0);
    // Three gears in mesh, each turning at the speed its teeth allow, on a
    // soft radial light, with a few twinkles.
    const float k = st.r / 150.0f, cx = st.cx, cy = st.cy;
    disc_rgradient(g, cx, cy, st.r, 0xF7F9FC, 0xD6DEEA);
    const float big_r = 62 * k, mid_r = 42 * k, small_r = 34 * k;
    const int big_n = 12, mid_n = 8, small_n = 7;
    const float bx = cx - 24 * k, by = cy + 12 * k;
    const float mid_a = -48 * DEG, small_a = 38 * DEG;
    const float mid_d = big_r + mid_r - 9 * k, small_d = big_r + small_r - 9 * k;
    const float mx = bx + cosf(mid_a) * mid_d, my = by + sinf(mid_a) * mid_d;
    const float sx = bx + cosf(small_a) * small_d, sy = by + sinf(small_a) * small_d;
    const float turn = clock * 0.35f;
    // Mesh: the smaller gears turn the other way, faster by the ratio of teeth,
    // and start half a tooth out of phase on the line between the centres.
    const float mid_turn = -turn * big_n / mid_n + mid_a * (1 + (float)big_n / mid_n) + 3.14159265f / mid_n;
    const float small_turn = -turn * big_n / small_n + small_a * (1 + (float)big_n / small_n) + 3.14159265f / small_n;
    gear(g, mx, my, mid_r, mid_n, mid_turn, k, 0xE9A82E, 0xB98118, 0xF7CB6A);
    gear(g, sx, sy, small_r, small_n, small_turn, k, 0x3FA7A0, 0x2B7F79, 0x7CCFC8);
    gear(g, bx, by, big_r, big_n, turn, k, 0x5F6B7D, 0x434D5B, 0x8692A4);
    for (int i = 0; i < 3; i++) {
        static const float POS[3][2] = {{-78, -70}, {82, 4}, {-70, 86}};
        const float tw = fabsf(sinf(clock * 1.7f + i * 2.1f));
        twinkle(g, cx + POS[i][0] * k, cy + POS[i][1] * k, (4 + 7 * tw) * k, mix(0xD6DEEA, 0xFFFFFF, 0.4f + 0.6f * tw));
    }
    restore_ring(g);
}

bool draw_question(G *g, int deck, int card, int index, int count, bool hint)
{
    const Card c = deck_card(deck, card);
    g->fillScreen(BG);
    draw_ring(g, count, index, c.color);
    return draw_title_block(g, "", "?", false, hint ? c.gloss : "", c.color, false);
}

bool draw_answer(G *g, int deck, int card, int index, int count)
{
    const Card c = deck_card(deck, card);
    g->fillScreen(BG);
    draw_ring(g, count, index, c.color);
    const bool fits = draw_title_block(g, c.lead, c.headword, true, c.gloss, c.color, false);
    // The two answers, next to their keys and in the keys' colours.
    const int y = CY + 4, xl = 34, xr = SCREEN - 34;
    line(g, xl - 8, y - 8, xl + 8, y + 8, 5, KEY_A);
    line(g, xl - 8, y + 8, xl + 8, y - 8, 5, KEY_A);
    line(g, xr - 10, y, xr - 3, y + 8, 5, KEY_B);
    line(g, xr - 3, y + 8, xr + 10, y - 8, 5, KEY_B);
    return fits;
}

bool draw_setting(G *g, int index, int count, const char *name, const char *value, const char *hint)
{
    static const Font *const HEAD[] = {&f_de_xl, &f_de_l, &f_de_mb};
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    static const Font *const LOCAL_S[] = {&f_loc_s};
    g->fillScreen(BG);
    draw_ring(g, count, index, COVER_COLOR, false);
    bool fits = draw_single(g, name, MUTED, false, LOCAL, 2, 160);
    if (value == nullptr) {
        // The power symbol: a ring open at the top, and a bar through the gap.
        const int y = 232;
        const uint32_t col = bright(COVER_COLOR);
        g->fillArc(CX, y, 31, 24, -55, 235, col);
        circle(g, CX + 27.5f * cosf(-55 * DEG), y + 27.5f * sinf(-55 * DEG), 3.5f, col);
        circle(g, CX + 27.5f * cosf(235 * DEG), y + 27.5f * sinf(235 * DEG), 3.5f, col);
        line(g, CX, y - 38, CX, y - 8, 7, col);
    } else {
        // Values written in the locale's script use its font; the rest the big German one.
        const bool local = (unsigned char)value[0] >= 0x80;
        fits = draw_single(g, value, bright(COVER_COLOR), false, local ? LOCAL : HEAD, local ? 2 : 3, 232) && fits;
    }
    return draw_single(g, hint, DOT_OFF, false, LOCAL_S, 1, 305) && fits;
}

int face_count(int deck, int card)
{
    if (card == COVER) {
        return 1;
    }
    return deck_card(deck, card).has_face3() ? 3 : 2;
}

bool draw_face(G *g, int deck, int card, int face)
{
    if (card == COVER) {
        return draw_cover(g, deck);
    }
    const Card c = deck_card(deck, card);
    return face == 0 ? draw_front(g, deck, card, c) : face == 1 ? draw_example(g, deck, card, c) : draw_tip(g, deck, card, c);
}

namespace {

// A card's character acting on a disc of any size; looking a card up resolves
// its animation ids by name, so it is done once per card, not per frame.
void card_stage(G *g, int deck, int index, int cx, int cy, int r, float t, float clock)
{
    static Card card;
    if (s_stage_card_deck != deck || s_stage_card_index != index) {
        card = deck_card(deck, index);
        s_stage_card_deck = deck;
        s_stage_card_index = index;
    }
    character::Stage st = stage_at(cx, cy, r, card.background, card.color);
    stage_disc(g, st, card.anim.who);
    character::draw(g, st, card.anim, t, clock);
}

}  // namespace

void draw_stage(G *g, int deck, int index, float t, float clock)
{
    character::Stage st = stage_at(STAGE_CX, STAGE_CY, STAGE_R, 0, 0);
    character::Anim anim;
    if (index == COVER) {
        st.cy = COVER_CY;
        st.r = COVER_R;
        st.bg = COVER_BG;
        st.accent = COVER_COLOR;
        const int who = deck_info(deck).character;
        if (character::count(who, character::POSE) > 0) {
            anim = cover_anim(who);
        } else {
            // A character without a body (the things of a noun deck) shows the
            // deck's first card it draws itself.
            static int cover_deck = -1;
            static character::Anim first;
            if (cover_deck != deck) {
                character::anim_reset(first, who);
                for (int i = 0; i < deck_info(deck).cards; i++) {
                    const Card c = deck_card(deck, i);
                    if (c.anim.who == who) {
                        first = c.anim;
                        break;
                    }
                }
                cover_deck = deck;
            }
            anim = first;
        }
    } else {
        // Looking a card up resolves its animation ids by name: once per card, not per frame.
        static Card card;
        if (s_stage_card_deck != deck || s_stage_card_index != index) {
            card = deck_card(deck, index);
            s_stage_card_deck = deck;
            s_stage_card_index = index;
        }
        st.bg = card.background;
        st.accent = card.color;
        anim = card.anim;
    }

    stage_disc(g, st, anim.who);
    character::draw(g, st, anim, t, clock);
    restore_ring(g);

}

void stage_rect(int &x, int &y, int &size)
{
    x = s_stage_x;
    y = s_stage_y;
    size = s_stage_size;
}

void set_clock(int hour, int minute, int second)
{
    s_hour = hour;
    s_minute = minute;
    s_second = second;
}

// ---- clock ----------------------------------------------------------------------

namespace {
// CLOCK_STAGE_MAX: the picture overshoots a little when it pops up; the square
// cleared and pushed every frame must hold it then too (and stay clear of the
// seconds ticks at the bottom).
constexpr int CLOCK_STAGE_CY = 314, CLOCK_STAGE_R = 122, CLOCK_STAGE_MAX = 128;
}

bool draw_clock(G *g, const ClockFace &f)
{
    static const Font *const DIGITS[] = {&f_clock};
    static const Font *const DATE[] = {&f_loc_s};
    static const Font *const SMALL[] = {&f_de_s};
    static const Font *const WORD[] = {&f_de_xl, &f_de_l, &f_de_mb};
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    g->fillScreen(BG);
    s_ring.count = 0;
    // The seconds, as 60 ticks round the edge: those that have passed light up.
    for (int i = 0; i < 60; i++) {
        const float a = (i * 6 - 90) * DEG, r0 = i % 5 ? 222 : 215;
        const uint32_t col = i <= f.second ? (i % 5 ? bright(COVER_COLOR) : mix(COVER_COLOR, WHITE, 0.65f)) : RING_OFF;
        line(g, CX + cosf(a) * r0, CY + sinf(a) * r0, CX + cosf(a) * 230, CY + sinf(a) * 230, i % 5 ? 3.0f : 4.5f, col);
    }
    char hm[24];
    snprintf(hm, sizeof(hm), "%d:%02d", f.hour, f.minute);
    bool fits = draw_single(g, hm, TEXT, false, DIGITS, 1, 100);
    fits = draw_single(g, f.date, MUTED, false, DATE, 1, 160) && fits;
    if (f.card >= 0 && f.show_word) {
        // In the place of the picture: the word, large, and what it means.
        const Card c = deck_card(f.deck, f.card);
        const bool lead = c.lead[0] != 0;
        // Centred where the picture was.
        if (lead) {
            fits = draw_single(g, c.lead, MUTED, false, SMALL, 1, 270) && fits;
        }
        fits = draw_single(g, c.headword, bright(c.color), true, WORD, 3, lead ? 310 : 300) && fits;
        fits = draw_single(g, c.gloss, MUTED, false, LOCAL, 2, lead ? 358 : 350) && fits;
        if (f.word_alpha < 0.99f) {
            // Fading: the word under a veil of the background.
            g->fillRectAlpha(80, 240, SCREEN - 160, 140, (uint8_t)(255 * (1 - fmaxf(0, f.word_alpha))), BG);
        }
    }
    return fits;
}

void draw_clock_stage(G *g, const ClockFace &f, float t, float clock)
{
    if (f.card >= 0) {
        if (!f.show_word) {
            // The square of the largest picture is cleared and pushed every
            // frame, so that a growing or shrinking picture is never cut off
            // and leaves nothing behind.
            const int x = CX - CLOCK_STAGE_MAX - 1, y = CLOCK_STAGE_CY - CLOCK_STAGE_MAX - 1, size = CLOCK_STAGE_MAX * 2 + 3;
            // Cleared every frame: a frame drawn at full size must also wipe what
            // the overshooting frame before it left at the four sides.
            const int r = (int)lroundf(fminf(CLOCK_STAGE_MAX, CLOCK_STAGE_R * f.stage_scale));
            g->fillRect(x, y, size, size, BG);
            if (r >= 4) {
                card_stage(g, f.deck, f.card, CX, CLOCK_STAGE_CY, r, t, clock);
            }
            s_stage_x = x;
            s_stage_y = y;
            s_stage_size = size;
        }
    } else {
        menu_icon(g, stage_at(CX, CLOCK_STAGE_CY, CLOCK_STAGE_R, 0, 0), ICON_CLOCK, clock);
    }
}

// ---- focus timer ------------------------------------------------------------------

namespace {
constexpr uint32_t FOCUS = 0xE5484D, BREAK = 0x5E9E45;
constexpr int TIMER_STAGE_CY = 338, TIMER_STAGE_R = 80;
}

bool draw_timer(G *g, const TimerFace &f)
{
    static const Font *const DIGITS[] = {&f_clock};
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    static const Font *const HINT[] = {&f_loc_s};
    const uint32_t accent = f.focus ? FOCUS : BREAK;
    g->fillScreen(BG);
    s_ring.count = 0;
    // The time left is the ring, running down anticlockwise from the top.
    g->fillArc(CX, CY, 231, 225, 0, 360, RING_OFF);
    const float left = f.total > 0 ? (float)f.remaining / f.total : 0;
    if (left > 0) {
        g->fillArc(CX, CY, 233, 221, -90, -90 + 360 * left, f.running ? accent : mix(accent, BG, 0.45f));
    }
    char mmss[24];
    snprintf(mmss, sizeof(mmss), "%02d:%02d", f.remaining / 60, f.remaining % 60);
    bool fits = draw_single(g, f.label, bright(accent), false, LOCAL, 2, 92);
    fits = draw_single(g, mmss, f.running ? TEXT : MUTED, false, DIGITS, 1, 156) && fits;
    return draw_single(g, f.hint, MUTED, false, HINT, 1, 222) && fits;
}

void draw_timer_stage(G *g, const TimerFace &f, float t, float clock)
{
    character::Stage st = stage_at(CX, TIMER_STAGE_CY, TIMER_STAGE_R, f.focus ? 0xFDEBE5 : 0xE8F4E2, f.focus ? FOCUS : BREAK);
    const character::Anim &a = timer_anim(f.focus, f.running);
    stage_disc(g, st, a.who);
    character::draw(g, st, a, t, clock);
}

// ---- messages and celebrations ------------------------------------------------------

namespace {
constexpr int MESSAGE_CY = 178, MESSAGE_R = 138;
}

bool draw_message(G *g, const char *german, const char *local, uint32_t accent)
{
    static const Font *const TITLE[] = {&f_de_xl, &f_de_l};
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    g->fillScreen(BG);
    draw_ring(g, 1, 0, accent, false);
    const bool fits = draw_single(g, german, bright(accent), false, TITLE, 2, 352);
    return draw_single(g, local, MUTED, false, LOCAL, 2, 400) && fits;
}

void draw_celebration_stage(G *g, int variant, float t, float clock)
{
    uint32_t bg, accent;
    const character::Anim &a = celebration_anim(variant % CELEBRATIONS, bg, accent);
    character::Stage st = stage_at(CX, MESSAGE_CY, MESSAGE_R, bg, accent);
    stage_disc(g, st, a.who);
    character::draw(g, st, a, t, clock);
    restore_ring(g);
}

bool draw_setting_edit(G *g, int index, int count, const char *name, const char *value, const char *hint)
{
    static const Font *const HEAD[] = {&f_de_xl, &f_de_l, &f_de_mb};
    static const Font *const LOCAL[] = {&f_loc_m, &f_loc_s};
    static const Font *const LOCAL_S[] = {&f_loc_s};
    g->fillScreen(BG);
    draw_ring(g, count, index, COVER_COLOR, false);
    bool fits = draw_single(g, name, MUTED, false, LOCAL, 2, 160);
    fits = draw_single(g, value, DOT_OFF, true, HEAD, 3, 232, bright(COVER_COLOR)) && fits;
    return draw_single(g, hint, MUTED, false, LOCAL_S, 1, 305) && fits;
}

}  // namespace ui
