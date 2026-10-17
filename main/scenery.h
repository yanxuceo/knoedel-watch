#pragma once
// Pieces shared by Brezel's props and the things of the noun decks (the
// mountain mini-series): the mini Knödel who comes along as the cat's buddy,
// the Dolomites skyline, fir trees, a chequered tablecloth.
// Drawing reference: buddy(), peaks(), fir(), tableCloth() in tools/preview/katze.js.

#include "character.h"
#include "vec.h"

namespace scenery {

// Knödel's expressions as a buddy.
enum Mood { HAPPY, WOW, TIRED, SHOCK, SLEEP, FLAT, COOL, CHEERS, POSE, WALK, FLOAT, FULL, HUG,
            COACH, STRETCH, LAUGH, ROLL, YOGA, SNOW };

// A small Knödel (about 32 units tall) with his feet at (0, 0) of `p`, moving
// by the clock. Drawn with Knödel's shapes and colours, at a third of his size.
void buddy(const vec::Pen &p, Mood mood, float clock, const character::Stage &st);

struct Palette {
    uint32_t rock, edge, shade, snow, hill, hill2;
};
extern const Palette DAY, DUSK;

// Rocky towers with snow and two green hills; the ground line is y = 0, x from -120 to 150.
void peaks(const vec::Pen &p, const Palette &pal);
void fir(const vec::Pen &p, float x, float y, float s, uint32_t col = 0x3E8E5A);
// A round leafy tree, its foot at (x, y).
void tree(const vec::Pen &p, float x, float y, float s, uint32_t col = 0x5BAF74);
// Red and white checks from (-120, -36) to (160, 44).
void table_cloth(const vec::Pen &p);
// The four-pointed sparkle of the previews (6 units), at the origin of `p`.
void star(const vec::Pen &p, uint32_t col);
// The stage font nearest to a text of `px` pixels.
const lgfx::IFont *font_for(const character::Stage &st, float px);

}  // namespace scenery
