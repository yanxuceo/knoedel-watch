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

// The memo as a note, the JSON {"kind": "todo"|"idea", "items": [{"text",
// "when": {"type": "none"|"days"|"weekday"|"date", "days", "weekday",
// "next_week", "date"}}], "text"}: how each task's day was named, for
// notes_sync.cpp to count. `context` tells the day it was said
// ("Today: Saturday, 2026-10-31"). Empty when it failed.
std::string tidy(const std::string &transcript, const char *context);

// Between these the requests share one connection: TLS's handshake, a second
// or two on the watch, is done once for all the memos of a go.
void begin_session();
void end_session();

// A tiny request, to see whether the key works: its HTTP status (200 good,
// 401 a wrong key, 429 the limit reached or too fast, 0 no answer).
int check_key();

// The HTTP status of the last request, 0 when it got no answer at all.
// 429 with spend_limited(): the project's monthly limit is reached.
int status();
bool spend_limited();

}  // namespace openai
