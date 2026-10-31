#pragma once
// The notes page (main menu > Notes): the notes and the memos still waiting,
// newest first, one at a time; tasks ticked by a tap, a ring menu on the card.
// The data are memos.h's files, the drawing notes_ui.h.

#include <M5GFX.h>

#include <string>

namespace notes_page {

// Reads the notes and the memos waiting again; keeps the place when it can.
void reload();

// Previous or next card.
void step(int dir);

void draw(lgfx::LovyanGFX *g, float clock);

// A tap on the screen at (x, y) (screen pixels of ui::SCREEN): ticks a task,
// opens or answers the menu. True when the page changed.
bool tap(int x, int y);

// The menu is open (the back key closes it).
bool menu_open();
void close_menu();

// Something moves (a note coming in, a memo being sent, Knödel cheering):
// the page is redrawn every frame.
bool animating();

// A note was just made from a memo (its JSON as notes_sync.h gives it):
// shown first, its text coming in.
void arrived(const std::string &note_json);

// A short line about a note (its JSON as notes_sync.h gives it), for the
// message "Noted: ...".
std::string summary(const std::string &note_json);

}  // namespace notes_page
