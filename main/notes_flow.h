#pragma once
// Holding the right key, anywhere: a voice memo is recorded while it is held,
// kept, and turned into a note when the watch gets online (memos.h,
// notes_sync.h). The screens here are plain placeholders until the notes get
// their own pages.

namespace notes_flow {

// Records while the right key is held, then sends what is waiting. Leaves the
// screen to be redrawn by the caller.
void talk();

}  // namespace notes_flow
