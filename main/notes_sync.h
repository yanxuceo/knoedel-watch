#pragma once
// Turns the memos waiting in memos.h into notes: online, oldest first, each
// transcribed and tidied (openai.h), its note written, its recording deleted.
// A memo that fails stays for the next time; the ones after it wait too.

#include <string>

namespace notes_sync {

struct Result {
    int done = 0;       // memos turned into notes
    int left = 0;       // still waiting
    bool offline = false;  // no saved network answered
    bool limited = false;  // the OpenAI project's monthly limit is reached
    std::string last;   // the newest note written (JSON), for a short message
};

// `progress` (may be null) is told before each memo: its place and how many there are.
Result run(void (*progress)(int index, int count) = nullptr);

}  // namespace notes_sync
