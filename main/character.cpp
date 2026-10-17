#include "character.h"

#include <string.h>

#include "gfx_util.h"
#include "dinge.h"
#include "katze.h"
#include "knoedel.h"

using namespace gfxu;

namespace character {

namespace {

const Impl *const IMPLS[] = {&knoedel::IMPL, &katze::IMPL, &dinge::IMPL};
constexpr int IMPL_COUNT = sizeof(IMPLS) / sizeof(IMPLS[0]);

int lookup(int who, Kind kind, const char *id)
{
    for (int i = 0; i < IMPLS[who]->count(kind); i++) {
        if (strcmp(IMPLS[who]->name(kind, i), id) == 0) {
            return i;
        }
    }
    return -1;
}

bool add(int16_t *list, int8_t &n, int index)
{
    if (index < 0 || n >= MAX_LAYERS) {
        return false;
    }
    list[n++] = (int16_t)index;
    return true;
}

// White circle of 35 % opacity (6 % on a night card), cut off at the edge of the disc.
void bg_blob(G *g, const Stage &st, float cx, float cy, float r)
{
    const bool dark = ((st.bg >> 16) & 0xFF) + ((st.bg >> 8) & 0xFF) + (st.bg & 0xFF) < 300;
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++) {
        float dy = y + 0.5f - cy, ddy = y + 0.5f - st.cy, lim = st.r - 1.5f;
        if (fabsf(dy) >= r || fabsf(ddy) >= lim) {
            continue;
        }
        float hw = sqrtf(r * r - dy * dy), dhw = sqrtf(lim * lim - ddy * ddy);
        int x0 = (int)lroundf(fmaxf(cx - hw, st.cx - dhw)), x1 = (int)lroundf(fminf(cx + hw, st.cx + dhw));
        if (x1 > x0) {
            g->fillRectAlpha(x0, y, x1 - x0, 1, dark ? 15 : 89, 0xFFFFFFu);
        }
    }
}

}  // namespace

int count() { return IMPL_COUNT; }

int find(const char *id)
{
    for (int i = 0; i < IMPL_COUNT; i++) {
        if (strcmp(IMPLS[i]->id, id) == 0) {
            return i;
        }
    }
    return -1;
}

const char *id(int who) { return IMPLS[who]->id; }
int count(int who, Kind kind) { return IMPLS[who]->count(kind); }
const char *name(int who, Kind kind, int index) { return IMPLS[who]->name(kind, index); }
float pose_neutral(int who, int index) { return IMPLS[who]->pose_neutral(index); }

void anim_reset(Anim &a, int who)
{
    a.who = (int8_t)who;
    for (int i = 0; i < MAX_POSE; i++) {
        a.pose[i] = i < IMPLS[who]->count(POSE) ? IMPLS[who]->pose_neutral(i) : 0;
    }
    a.tint = 0;
    a.has_tint = false;
    a.n_motion = a.n_fx = a.n_prop = 0;
    a.bubble = -1;
}

bool anim_set_pose(Anim &a, const char *name, float value)
{
    const int i = lookup(a.who, POSE, name);
    if (i < 0) {
        return false;
    }
    a.pose[i] = value;
    return true;
}

bool anim_add_motion(Anim &a, const char *id) { return add(a.motion, a.n_motion, lookup(a.who, MOTION, id)); }
bool anim_add_fx(Anim &a, const char *id) { return add(a.fx, a.n_fx, lookup(a.who, FX, id)); }
bool anim_add_prop(Anim &a, const char *id) { return add(a.prop, a.n_prop, lookup(a.who, PROP, id)); }

bool anim_set_bubble(Anim &a, const char *id)
{
    const int i = lookup(a.who, BUBBLE, id);
    if (i < 0) {
        return false;
    }
    a.bubble = (int8_t)i;
    return true;
}

void draw_background(G *g, const Stage &st, int who)
{
    g->fillSmoothCircle(st.cx, st.cy, st.r, st.bg);
    const float u = st.r / 100.0f;
    for (auto &b : IMPLS[who]->blobs) {
        bg_blob(g, st, st.cx + (b[0] - 100) * u, st.cy + (b[1] - 100) * u, b[2] * u);
    }
}

void draw(G *g, const Stage &st, const Anim &anim, float t, float clock)
{
    // Nothing may land outside the square around the disc: only that square is redrawn.
    const int x0 = st.cx - st.r - 1, y0 = st.cy - st.r - 1, size = st.r * 2 + 3;
    g->setClipRect(x0, y0, size, size);
    IMPLS[anim.who]->draw(g, st, anim, t, clock);
    if (IMPLS[anim.who]->clip) {
        // Inside the square, what lies beyond the disc goes back to the screen's black.
        const float r = st.r + 0.5f;
        for (int y = y0; y < y0 + size; y++) {
            const float dy = y + 0.5f - st.cy;
            const int hw = fabsf(dy) < r ? (int)floorf(sqrtf(r * r - dy * dy)) : -1;
            const int left = hw < 0 ? x0 + size : st.cx - hw, right = hw < 0 ? x0 : st.cx + hw + 1;
            g->fillRect(x0, y, left - x0, 1, 0x000000u);
            g->fillRect(right, y, x0 + size - right, 1, 0x000000u);
        }
    }
    g->clearClipRect();
}

}  // namespace character
