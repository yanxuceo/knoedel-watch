#pragma once
// Knödel Orbit on the watch: orbit_game.h with the keys, the motion sensor,
// the speaker and the motor.

#include <M5GFX.h>

namespace orbit {

// Plays until the left key is held: tilting steers, the right key fires, the
// left key jumps, a shake sets off a bomb, lying face down pauses. On the
// title a tap switches the steering. Draws into `fb` (and a second frame of
// its own) and shows it at (ox, oy); leaves the screen to be redrawn by the caller.
void run(lgfx::LGFX_Sprite *fb, int ox, int oy);

}  // namespace orbit
