#pragma once
// Holding the right key, anywhere: a voice memo is recorded while it is held,
// kept, and turned into a note when the watch gets online (memos.h,
// notes_sync.h), with the screens of notes_ui.h.

#include <M5GFX.h>

namespace notes_flow {

// Records while the right key is held (the left key cancels), keeps the memo
// and asks for it to be sent. Draws into `fb` and shows it with `push`; leaves
// the screen to be redrawn by the caller.
void talk(lgfx::LovyanGFX *fb, void (*push)());

}  // namespace notes_flow
