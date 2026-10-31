#pragma once
// Turns the memos waiting in memos.h into notes, in a task of its own: online,
// oldest first, each transcribed and tidied (openai.h), its note written, its
// recording deleted. A memo that fails stays, and the ones after it wait too.
//
// Going online costs most in connecting, not in sending, so on battery the
// memos are sent together (saving battery, the default):
//   on USB power, the notes page open, or "at once" chosen: at once
//   else: when 3 are waiting, or the oldest has waited 15 minutes
//   below 20 % battery: only on USB power or with the notes page open
// After a failure it waits 1, 5, 15, then 60 minutes, unless asked again
// (a new memo, the notes page opened).

#include <stdint.h>

#include <string>

namespace notes_sync {

struct Result {
    int done = 0;          // memos turned into notes
    int left = 0;          // still waiting
    bool offline = false;  // no saved network answered
    bool limited = false;  // the OpenAI project's monthly limit is reached
    std::string last;      // the newest note written (JSON), for a short message
};

// Starts the task; it goes at once when memos are waiting from before.
void start();

// A memo was just kept: the task looks whether to go now.
void request();

// The notes page shows (its notes are wanted now) or not.
void page_open(bool open);

// Send in batches to save battery (true, the default) or each memo at once.
bool saving();
void set_saving(bool on);

// Whether the memos waiting would go now (for the message after recording).
bool goes_now();

// A go is under way (the watch then stays on).
bool busy();

// The last go that finished, and how many have (to notice a new one).
Result last(uint32_t *count = nullptr);

}  // namespace notes_sync
