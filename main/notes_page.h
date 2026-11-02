#pragma once
// The notes page (main menu > Notes): two lists, the open tasks of all notes
// (by day: late ones, today, tomorrow, ...; then those without a day) and the
// ideas (newest first), four rows a page. A tap ticks a task or opens an idea;
// holding a row opens a menu with its whole text (delete). A task ticked stays,
// greyed, until the next day.
//
// After the open tasks comes what was done: this week's count, a bar a day
// and the days in a row; a tap on it lists the tasks done (newest first, kept
// 90 days), where holding one can untick it. The data are memos.h's files,
// the drawing notes_ui.h.

#include <M5GFX.h>

#include <string>

namespace notes_page {

// Reads the notes and the memos waiting again; keeps the place when it can.
void reload();

// Previous or next page; past the last page of the tasks come the ideas.
void step(int dir);

void draw(lgfx::LovyanGFX *g, float clock);

// A tap or a hold at (x, y) (pixels of ui::SCREEN). True when the page changed.
bool tap(int x, int y);
bool hold(int x, int y);

// A finger dragging up or down by dy pixels: scrolls the text in the menu,
// for a long task. True when the page changed.
bool scroll(int dy);

// Tasks ticked today, for the main menu (reads the notes the first time).
int done_today();

// The menu, an idea or the list of what was done is open (the back key closes it).
bool menu_open();
void close_menu();

// Something moves (a note coming in, a memo being sent, Knödel cheering):
// the page is redrawn every frame.
bool animating();

// A note was just made from a memo (its JSON as notes_sync.h gives it): its
// rows come in where they belong.
void arrived(const std::string &note_json);

// A short line about a note (its JSON as notes_sync.h gives it), for the
// message "Noted: ...".
std::string summary(const std::string &note_json);

}  // namespace notes_page
