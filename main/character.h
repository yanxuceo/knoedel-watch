#pragma once
// The animated characters on the card stage, drawn from data.
//
// A card's animation is a target pose plus lists of motions, effects, props and
// an optional thought bubble, all referred to by the ids in
// registry/<character>.json. The drawing reference for every id is
// tools/preview/<character>.js. Each character's code is one file
// (knoedel.cpp, katze.cpp, dinge.cpp) that provides an Impl.

#include <M5GFX.h>

namespace character {

// Length of one neutral -> pose -> neutral loop, in seconds.
constexpr float LOOP_SECONDS = 7.4f;

constexpr int MAX_POSE = 24;  // pose parameters of the richest character
constexpr int MAX_LAYERS = 4;

enum Kind { POSE, MOTION, FX, PROP, BUBBLE, KIND_COUNT };

struct Anim {
    int8_t who;              // which character, see find()
    float pose[MAX_POSE];    // target pose, in the character's registry order
    uint32_t tint;           // colour the body blends towards (by the "tint" pose value)
    bool has_tint;
    int8_t motion[MAX_LAYERS], fx[MAX_LAYERS], prop[MAX_LAYERS];
    int8_t n_motion, n_fx, n_prop;
    int8_t bubble;           // -1: none
};

struct Stage {
    int cx, cy, r;                  // disc the character lives in (screen pixels)
    uint32_t bg;                    // disc colour
    uint32_t accent;                // the card's colour, used by some effects
    const lgfx::IFont *font_tiny;   // words inside thought bubbles
    const lgfx::IFont *font_small;  // "puh", "Hatschi!", "39°"
    const lgfx::IFont *font_big;    // "!"
};

// What one character's code provides.
struct Impl {
    const char *id;
    int (*count)(Kind kind);
    const char *(*name)(Kind kind, int index);
    float (*pose_neutral)(int index);
    float blobs[3][3];  // pale circles on the disc: x, y, r on the 200 x 200 disc
    bool clip;          // cut off at the edge of the disc (props that enter or leave the stage)
    void (*draw)(lgfx::LovyanGFX *g, const Stage &stage, const Anim &anim, float t, float clock);
};

int count();                 // characters
int find(const char *id);    // -1: unknown
const char *id(int who);

// What is implemented, for checking registry/<character>.json against the code.
int count(int who, Kind kind);
const char *name(int who, Kind kind, int index);
float pose_neutral(int who, int index);

void anim_reset(Anim &a, int who);  // neutral pose, nothing else
// Each returns false if the character's code does not implement that name.
bool anim_set_pose(Anim &a, const char *name, float value);
bool anim_add_motion(Anim &a, const char *id);
bool anim_add_fx(Anim &a, const char *id);
bool anim_add_prop(Anim &a, const char *id);
bool anim_set_bubble(Anim &a, const char *id);

// The disc the character stands in. It never changes, so callers can cache it.
void draw_background(lgfx::LovyanGFX *g, const Stage &stage, int who);

// Draws the character, its props, effects and bubble on top of the background.
// `t` is the position inside the loop (0..LOOP_SECONDS), `clock` a free-running
// time in seconds.
void draw(lgfx::LovyanGFX *g, const Stage &stage, const Anim &anim, float t, float clock);

}  // namespace character
