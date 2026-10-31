#pragma once
// Screens for a round screen of SCREEN pixels: main menu, settings, and the
// card faces (FORMAT.md, "Faces on the device").
//
//   cover : the deck's title, shown when a deck is opened
//   face 1: animated Knödel, headword, short meaning
//   face 2: grammar line, example sentence, translation
//   face 3: tip (and the pattern of verbs and nouns)

#include <M5GFX.h>

namespace ui {

// Every screen is laid out for a circle this wide, that of the M5Stack
// StopWatch. A board's screen may be a little smaller or larger: the app
// centres the picture on it (see main/board/board.h).
constexpr int SCREEN = 468;
constexpr int CX = SCREEN / 2, CY = SCREEN / 2;

// Area of the cover and of face 1 that changes every frame.
constexpr int STAGE_CX = CX, STAGE_CY = 174, STAGE_R = 136;
constexpr int STAGE_X = STAGE_CX - STAGE_R - 1, STAGE_Y = STAGE_CY - STAGE_R - 1, STAGE_SIZE = STAGE_R * 2 + 3;

// The menu pages have one line of text, so their picture is larger: picture
// and title form one group a little above the centre, with the title as low
// as its widest version still fits the narrowing circle.
constexpr int MENU_CY = 192, MENU_R = 150, MENU_TITLE_Y = 378;
constexpr int MENU_X = CX - MENU_R - 1, MENU_Y = MENU_CY - MENU_R - 1, MENU_SIZE = MENU_R * 2 + 3;

// Deck covers: German titles always have the same size and take a second line
// when they are long, so the picture is smaller, the same on every cover.
constexpr int COVER_CY = 146, COVER_R = 118;
constexpr int COVER_X = CX - COVER_R - 1, COVER_Y = COVER_CY - COVER_R - 1, COVER_SIZE = COVER_R * 2 + 3;

constexpr int COVER = -1;  // card index of a deck's cover

struct FontBlobs {
    // German: headword (two sizes), text, bold text, small, tiny; digits of the clock.
    const uint8_t *de_xl, *de_l, *de_m, *de_mb, *de_s, *de_xs, *clock;
};

void init(const FontBlobs &blobs);

// Call after every deck_init(): the fonts of that pack's locale (gloss size,
// translation size, menu title size). The locale also decides how text is wrapped.
void set_locale(const uint8_t *loc_m, const uint8_t *loc_s, const uint8_t *loc_l);

// Number of faces of a card (2 or 3); 1 for the cover.
int face_count(int deck, int card);

// Draws one whole face. Returns false if some text did not fit the round
// screen; the importer uses this to refuse such a deck.
bool draw_face(lgfx::LovyanGFX *g, int deck, int card, int face);

// The animated part of the cover and of face 1, drawn over what draw_face left.
void draw_stage(lgfx::LovyanGFX *g, int deck, int card, float t, float clock);

// ---- menus ----

enum MenuIcon { ICON_KNOEDEL, ICON_BREZEL, ICON_GEAR, ICON_CLOCK, ICON_TIMER, ICON_NOTES };

// One entry of the main menu, or a message: one title in the learner's
// language, and a ring that shows which of `count` entries this is; `sub`, a
// small grey line under the title (null: none).
bool draw_menu(lgfx::LovyanGFX *g, int index, int count, const char *title, const char *sub = nullptr);
void draw_menu_stage(lgfx::LovyanGFX *g, MenuIcon icon, float t, float clock);

// ---- practice ----

// The question: the card's character acts the word out (draw_stage), and no
// word is shown. `hint` adds the meaning in the learner's language. `index` of
// `count` is the place in the session.
bool draw_question(lgfx::LovyanGFX *g, int deck, int card, int index, int count, bool hint);
// The answer: face 1, with the marks of the two answer keys at the sides
// (left: not yet, right: knew it).
bool draw_answer(lgfx::LovyanGFX *g, int deck, int card, int index, int count);

// The square that the last draw_*stage call drew: the part of the screen that
// changes every frame.
void stage_rect(int &x, int &y, int &size);

// The time, for the clock pictures.
void set_clock(int hour, int minute, int second);

// ---- clock ----

// The watch face: time, date, and a word of the day. Below the date the word's
// character acts it out (draw_clock_stage), then the word itself takes its
// place (`show_word`), in turns. `card` < 0: no word, a picture of the time.
struct ClockFace {
    int hour, minute, second;
    const char *date;
    int deck, card;
    bool show_word;
    float word_alpha;   // the word fades in and out (0..1)
    float stage_scale;  // the picture grows in and shrinks away (0..1, may overshoot)
};
bool draw_clock(lgfx::LovyanGFX *g, const ClockFace &face);
void draw_clock_stage(lgfx::LovyanGFX *g, const ClockFace &face, float t, float clock);

// ---- focus timer ----

// The outer ring runs down with the time left; the character below works
// (focus) or relaxes (break).
struct TimerFace {
    int remaining, total;  // seconds
    bool focus, running;
    const char *label, *hint;
};
bool draw_timer(lgfx::LovyanGFX *g, const TimerFace &face);
void draw_timer_stage(lgfx::LovyanGFX *g, const TimerFace &face, float t, float clock);

// ---- messages and celebrations ----

// A big German word with a line in the learner's language below, under a
// large picture (draw_celebration_stage), like a menu page.
bool draw_message(lgfx::LovyanGFX *g, const char *german, const char *local, uint32_t accent);
constexpr int CELEBRATIONS = 6;  // variants of draw_celebration_stage
void draw_celebration_stage(lgfx::LovyanGFX *g, int variant, float t, float clock);

// A setting being edited: `value` is marked up, its {{field}} highlighted.
bool draw_setting_edit(lgfx::LovyanGFX *g, int index, int count, const char *name, const char *value, const char *hint);

// One setting, full screen: name, current value, hint. A null value shows the
// power symbol instead (the power-off item).
bool draw_setting(lgfx::LovyanGFX *g, int index, int count, const char *name, const char *value, const char *hint);

// ---- for screens drawn elsewhere (notes_ui.h) ----

// The fonts of the pack: German (headword sizes, text, small), the locale's
// (small, text, menu title) and the clock's digits.
enum class FontId { DE_XS, DE_S, DE_M, DE_MB, LOC_S, LOC_M, LOC_L, CLOCK };
const lgfx::IFont *font(FontId id);

// Knödel as a buddy (scenery.h), standing at (x, y) and `height` pixels tall.
void draw_buddy(lgfx::LovyanGFX *g, float x, float y, float height, int mood, float clock);

}  // namespace ui
