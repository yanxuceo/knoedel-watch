#pragma once
// Turns the memos waiting in memos.h into notes, in a task of its own: online,
// oldest first, each transcribed and tidied (openai.h), its note written, its
// recording deleted. A memo that fails stays, and the ones after it wait too:
// the task tries again after 1, 5, 15 and then every 60 minutes, and at once
// when asked (a new memo).

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

// Asks for a go soon (a memo was just kept).
void request();

// A go is under way (the watch then stays on).
bool busy();

// The last go that finished, and how many have (to notice a new one).
Result last(uint32_t *count = nullptr);

}  // namespace notes_sync
