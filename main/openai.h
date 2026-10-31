#pragma once
// The two requests behind a note (needs net::connect() and a key in
// credentials.h): the recording to text, then the text tidied into a note.
// Tried on the computer first with tools/mic_test.py, same models and prompts.

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace openai {

// The transcript of 16-bit mono samples; empty when it failed (see status()).
std::string transcribe(const int16_t *pcm, size_t samples, uint32_t rate);
// The same for an Ogg Opus file (memos.h).
std::string transcribe_ogg(const uint8_t *ogg, size_t size);

// The memo as a note, the JSON {"kind": "todo"|"idea", "items": [{"text", "due"}],
// "text"}; `now` like "Saturday, 2026-10-31, 10:45". Empty when it failed.
std::string tidy(const std::string &transcript, const char *now);

// The HTTP status of the last request, 0 when it got no answer at all.
// 429 with spend_limited(): the project's monthly limit is reached.
int status();
bool spend_limited();

}  // namespace openai
