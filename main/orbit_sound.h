#pragma once
// The sounds of Knödel Orbit, made by the program when the game first opens
// (no files): short effects and two music loops, as 16-bit mono samples.
// Pitched for a small speaker: nothing much below 150 Hz, where it is silent.

#include <stddef.h>
#include <stdint.h>

namespace orbit_sound {

enum Id { SHOOT, KILL, GRAZE, HIT, JUMP, BEAM, BOMB, COMBO, TICK, CHARGED, START, OVER, MUSIC_CALM, MUSIC_HOT, COUNT };

constexpr uint32_t RATE = 16000;

struct Clip {
    const int16_t *pcm;
    size_t samples;
};

// Makes them all, once (about 0.4 MB); false when there is no memory for them.
// `pause` (may be null) is called every so often, for making them in the
// background without starving the other tasks.
bool make(void (*pause)() = nullptr);

// An empty clip until make() has made it (they are made in the order of Id).
Clip clip(Id id);

}  // namespace orbit_sound
