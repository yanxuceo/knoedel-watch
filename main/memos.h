#pragma once
// Voice memos on the watch: recorded while the right key is held, kept as Ogg
// Opus in the "notes" partition (LittleFS) until they are online, then turned
// into notes (notes_sync.h). A memo's file goes only when its note is written.
//
//   /notes/memos/<id>.ogg   waiting to go online
//   /notes/done/<id>.json   the note made of it (with its transcript)
//
// <id> is when it was recorded, "20261031-111508", so names sort by time.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace memos {

static constexpr uint32_t RATE = 16000;
static constexpr float MAX_SECONDS = 60;

// Mounts the partition (formats it the first time). False when it cannot.
bool begin();

// ---- recording (all in the caller's task) ------------------------------------

// Starts the microphone, the encoder and a new file.
bool record_start();
// Call often while recording (every few ms): fetches what the microphone
// filled, encodes and writes it. False once MAX_SECONDS are reached.
bool record_poll();
float record_seconds();
// 0..1, how loud the newest tenth of a second was, for a level meter.
float level();
// Ends the recording. `keep` false (cancelled, too short) deletes the file.
// Returns the memo's id, or "" when nothing was kept.
std::string record_stop(bool keep);

// ---- what is stored -------------------------------------------------------------

// Memos waiting to go online, oldest first.
std::vector<std::string> waiting();
// The memo's Ogg Opus file, read whole into PSRAM (free() it). nullptr if missing.
uint8_t *read(const std::string &id, size_t &size);
// Writes the note (atomically: a temporary file, then a rename), and only then
// deletes the memo's recording.
bool finish(const std::string &id, const std::string &note_json);
// Notes, newest first.
std::vector<std::string> notes();
std::string note(const std::string &id);

// Bytes used and the size of the partition.
void usage(size_t &used, size_t &total);

// The gain applied to the raw samples, with a soft limit near full scale.
void apply_gain(int16_t *pcm, size_t n);

}  // namespace memos
