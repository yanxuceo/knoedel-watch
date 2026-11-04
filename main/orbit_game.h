#pragma once
// Knödel Orbit, the game (main menu > Knödel Orbit): the round screen is the
// arena. A ship runs on the rim, steered by tilting the watch; Sauerkraut comes
// out of the vortex in the middle and shoots outward. The right key fires (held
// and let go: a beam through the vortex), the left key jumps through the middle
// to the other side, a shake of the wrist sets off a bomb. Bullets passing close
// by ("grazes") score and keep the combo going.
//
// This is the game alone: its rules and its drawing, with no board in it, so
// that the desktop sim can draw it too. orbit.h runs it on the watch.

#include <M5GFX.h>
#include <stdint.h>

#include "orbit_sound.h"
#include "ui_strings.h"

namespace orbit {

// What the player does, read once per frame.
struct Input {
    float tilt_x = 0, tilt_y = 0;  // the way the watch leans, in screen directions, -1..1 each
    bool fire = false;             // the right key is down now
    bool jump = false;             // the left key was pressed (since the last frame)
    bool bomb = false;             // the watch was shaken
};

// What the game asks of the watch: a sound (orbit_sound.h), the music (0 none,
// 1 calm, 2 hot), and the motor (`pattern`: on, off, on, ... in ms, ending in 0).
struct Effects {
    void (*sound)(orbit_sound::Id id);
    void (*music)(int level);
    void (*buzz)(const uint16_t *pattern, uint8_t level);
};

enum State { TITLE, PLAY, OVER };

// Starts on the title screen; `best` is the best score so far.
void begin(const Effects &fx, uint32_t best, uint32_t seed);

// One frame, `dt` seconds after the last. On the title and after a game, the
// right key starts the next one.
void update(float dt, const Input &in);

void draw(lgfx::LovyanGFX *g, float clock, const UiText &tx);

State state();
bool paused();
void set_paused(bool on);

// Steering: a marble rolling along the rim toward the lean (true), or turned
// left and right like a wheel.
void set_marble(bool on);

uint32_t best();

// How busy the screen is, for the log: enemies, bullets, particles.
void counts(int &enemies, int &pellets, int &particles);

// Where drawing spends its time, for the log: given a clock in microseconds,
// draw() adds up its parts (the back: stars and vortex; the things in play;
// the ship and texts; the score and overlays), and profile() hands them over
// and starts again.
constexpr int PROFILE_PARTS = 4;
void set_clock(uint64_t (*micros)());
void profile(uint32_t us[PROFILE_PARTS]);

// The game's picture in the main menu: the vortex with a ship going round it.
void draw_icon(lgfx::LovyanGFX *g, float cx, float cy, float r, float clock);

}  // namespace orbit
