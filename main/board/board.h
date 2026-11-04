#pragma once
// What the app needs from a board, and all it may touch of its hardware.
//
// Every board is an ESP32-S3 with PSRAM, a round touch screen, at least two keys
// and a speaker; a motion sensor and a real-time clock are optional. One file
// in main/board/ implements this header for each board, chosen in Kconfig
// ("Target board"). The app draws with M5GFX (LovyanGFX) on the board's display.
//
// What does not belong here: drawing, decks, content, settings and timing.
// They are the same on every ESP32-S3 and stay in the app.

#include <M5GFX.h>
#include <stdint.h>

namespace board {

// Starts the display, touch, keys, speaker and sensors. NVS is ready by then.
void begin();

// ---- screen -------------------------------------------------------------------

// Round, 16-bit colour. Brightness (0..255) and sleep() / wakeup() work as
// in LovyanGFX. The app draws for a 468 px circle (ui::SCREEN) and centres it
// on this display: a larger screen gets a black rim, a smaller one loses the
// outermost pixels of the ring. Screens from 460 to 480 px work well.
lgfx::LGFX_Device &display();

// ---- input --------------------------------------------------------------------

// What the user asked for, independent of the key or gesture used.
enum class Event : uint8_t {
    NONE,
    PREVIOUS,  // the left key, or a swipe to the right
    NEXT,      // the right key, or a swipe to the left
    SELECT,    // a tap on the screen
    BACK,      // holding the left key
    TALK,      // holding the right key: talk while it is held (see talk_key_held)
    HOLD,      // a finger held on the screen (tap_point tells where)
};

// Call once per frame: reads keys and touch, returns at most one event.
Event update();

// A key or the screen is being touched right now (keeps the watch awake).
bool touched();

// Where the last SELECT or HOLD touched, in display pixels.
void tap_point(int &x, int &y);

// A key is down at the last update() (left: the one that goes back).
bool key_down(bool right);

// While a finger drags up or down: how far it moved since the last update(),
// in pixels (down is positive); 0 otherwise.
int scroll_dy();

// The left key is held down (as read by the last talk_key_held()): cancels a recording.
bool cancel_key_held();

// Waits until no key is held, and drops the presses that ended meanwhile, so
// that a key used while recording does not also count as a click.
void wait_keys_released();

// ---- sound --------------------------------------------------------------------

void set_volume(uint8_t volume);  // 0..255

// Plays 16-bit samples, replacing what was playing. The board reads `pcm`
// until the sound ends or stop_sound() returns.
void play_pcm(const int16_t *pcm, size_t samples, uint32_t rate, bool stereo);

// Stops all sound, the one of play_pcm() and tones.
void stop_sound();

// A sound of the app's own on `channel` (1..7), mixed with the other channels
// where the board can (else only channel 1 plays, replacing what played);
// `loop`: over and over until stop_sound(). `pcm` stays valid while it plays.
void play_sound(const int16_t *pcm, size_t samples, uint32_t rate, uint8_t channel, bool loop);

// A beep for the app's little tunes; it plays alongside play_pcm(). Tones on
// different channels (1..3) play together where the board can mix them; a board
// that cannot plays only channel 1.
void tone(uint16_t hz, uint16_t ms, uint8_t channel = 1);

// ---- vibration (optional) ------------------------------------------------------

bool has_vibration();

// Runs the motor at `level` (0..255) until it is called again; 0 stops it.
void vibrate(uint8_t level);

// ---- microphone (optional) ------------------------------------------------------

bool has_mic();

// The right key is held down right now (reads the keys afresh).
bool talk_key_held();

// Starts the microphone. Sound stops until mic_end(): on some boards the
// speaker and the microphone share one codec, which may also need up to a
// second after starting before its samples are more than silence.
bool mic_begin();

// Queues `samples` 16-bit mono samples at `rate` to be filled into `pcm` in
// the background. Queued requests follow each other without a gap. False
// while two are waiting.
bool mic_record(int16_t *pcm, size_t samples, uint32_t rate);

// Requests queued and not filled yet: 0, 1 or 2.
int mic_busy();

// Waits for the queued requests, stops the microphone and gives the sound back.
void mic_end();

// Starts the microphone once and waits until it delivers sound (up to about a
// second), so that the first recording does not lose its first words. Call it
// once after starting, when no sound plays.
void mic_warm_up();

// ---- motion sensor (optional) ---------------------------------------------------

// Acceleration in g, z pointing out of the screen: a watch lying face up
// reads about (0, 0, 1). False without a sensor.
bool has_motion();
bool acceleration(float &x, float &y, float &z);

// ---- clock (optional) -----------------------------------------------------------

struct DateTime {
    int16_t year;  // 2000..2099
    int8_t month, day, hour, minute, second;
};

bool has_clock();
// False without a clock, or if it holds no valid time (it never ran, or lost power).
bool get_time(DateTime &t);
bool set_time(const DateTime &t);

// ---- power --------------------------------------------------------------------

// Powered over USB: the watch then stays on.
bool on_usb();

// The battery's charge, 0..100, or -1 when the board cannot tell.
int battery_level();

// Switches the watch off; returns only where it cannot be (on USB power).
void power_off();

}  // namespace board
