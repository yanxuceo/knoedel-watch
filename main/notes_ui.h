#pragma once
// The screens of the notes (voice memos), laid out for the round screen like
// card_ui.h: the recording, the notes page, its menu, and the messages that
// show over any screen. Drawing only: what to show comes from notes_page.h and
// notes_flow.h, so the simulator can draw every state (sim/sim_main.cpp).
// The design: the notes prototype (a canvas on claude.ai), 1:1 at 468 px.

#include <M5GFX.h>
#include <stdint.h>

namespace notes_ui {

// The font for what was said: the common Chinese characters and Latin, 24 px
// (font_notes.vlw in the content pack).
void init(const uint8_t *notes_font);

// The texts of the learner's language (ui_strings.h).
void set_locale(const char *locale);

// ---- the notes page -----------------------------------------------------------------

struct Item {
    const char *text;
    const char *due;  // already a label ("明天", "周五"), or nullptr
    bool done;
};

enum Kind { TODO, IDEA, WAITING, SENDING };

struct Card {
    Kind kind;
    const char *when;  // "今天 14:32"
    const Item *items;
    int item_count;
    const char *text;  // an idea's text
    int reveal;        // characters shown so far, as they come in; -1: all
    float seconds;     // a recording's length (WAITING, SENDING)
};

struct Status {
    bool online;    // a go online is under way
    int waiting;    // memos not yet notes
    bool limited;   // the OpenAI project's monthly limit
    bool no_key;    // no API key on the watch
};

// One card of `count` (index from 0), or the empty page when `card` is null.
// The notes page shows lists (below); a card is an idea opened from its list.
void draw_page(lgfx::LovyanGFX *g, const Card *card, int index, int count, const Status &status, float clock);

// The notes page: the open tasks of all notes, or the ideas, four rows a page.
// Rows are tall and far apart, so that a finger finds the one it means.
enum List { TASKS, IDEAS, DONE };
struct Row {
    const char *text;
    const char *due;  // a label, or nullptr
    bool done;
    int reveal;       // characters shown as it comes in; -1: all
};
struct ListPage {
    List list;
    int tasks, ideas, done;  // how many there are, for the tabs
    const Row *rows;
    int row_count;
    int page, pages;
    int held;          // the row held down, shown selected; -1: none
};
static constexpr int ROWS_PER_PAGE = 4;
void draw_list(lgfx::LovyanGFX *g, const ListPage &p, const Status &status, float clock);
// A tap or hold on the list page: a row (0..3), or -1; `tab` gets the tab tapped, or -1.
int row_at(int x, int y, int &tab);

// What was done, the page after the open tasks: this week's count, a bar a
// day, the days in a row with something done.
struct Stats {
    int week[7];  // Monday first
    int today;    // 0..6, Monday first
    int streak;   // days in a row up to today (or yesterday) with something done
    int tasks, ideas;
    int page, pages;
};
void draw_stats(lgfx::LovyanGFX *g, const Stats &s, const Status &status);

// What a tap at (x, y) on the page meant: an item's index, or -1 (the card
// itself: the menu). Uses the layout of the last draw_page.
int item_at(int x, int y);

// The ring menu over the page: `first` (ALL_DONE, UNDO, or NOTHING for none),
// delete, cancel; `title`, the row held (null: "this note").
enum Choice { NOTHING, ALL_DONE, UNDO, REMOVE, CANCEL };
void draw_menu(lgfx::LovyanGFX *g, Choice first, const char *title = nullptr);
Choice menu_at(Choice first, int x, int y);

// ---- recording ------------------------------------------------------------------------

// While the key is held: the sound around the edge, the time, the hints.
void draw_recording(lgfx::LovyanGFX *g, float seconds, float max_seconds, float level, float clock);

// After letting go, for a moment; `t` runs from 0 to 1.
enum Message { KEPT_ONLINE, KEPT_LATER, KEPT_OFFLINE, KEPT_NO_KEY, TOO_SHORT, CANCELLED };
void draw_message(lgfx::LovyanGFX *g, Message m, float t);

// ---- over any screen ------------------------------------------------------------------

// "已记下：..." near the bottom.
void draw_toast(lgfx::LovyanGFX *g, const char *text);
// The rectangle the toast covers, to redraw only that.
void toast_rect(int &x, int &y, int &w, int &h);

// Knödel comes up from the bottom and cheers: all of a note's tasks are done.
// `t` in seconds since it started.
void draw_cheer(lgfx::LovyanGFX *g, float t, float clock);
static constexpr float CHEER_SECONDS = 1.8f;

}  // namespace notes_ui
