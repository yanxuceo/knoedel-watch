#include "orbit_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "card_ui.h"
#include "gfx_util.h"

namespace orbit {

using G = lgfx::LovyanGFX;

static constexpr float PI = 3.14159265f, TAU = 2 * PI;
static constexpr float CX = ui::CX, CY = ui::CY;
static constexpr float R = 192;  // the rim the ship runs on

static constexpr uint32_t BLACK = 0x000000u, INK = 0xEFEAF7u, MUTED = 0x8D86A0u, CYAN = 0x4FF3FFu, AMBER = 0xFFC24Au,
                          HOT = 0xFF3D7Fu, KRAUT = 0xD9E86Au, DART = 0xFF8C5Au, PELLET = 0xEBFFC8u, STAR = 0xC8C8FFu,
                          VIOLET = 0xB98BFFu;

// ---- sizes of things --------------------------------------------------------------

static constexpr int MAX_ENEMIES = 64, MAX_PELLETS = 48, MAX_SHOTS = 24, MAX_PARTS = 180, MAX_TEXTS = 8,
                     MAX_WAVES = 4, MAX_QUEUE = 40, STARS = 96;
static constexpr float JUMP_S = 0.24f, JUMP_COOLDOWN = 1.3f, COMBO_S = 2.2f, CHARGE_S = 0.85f, BEAM_S = 0.38f;

struct Enemy {
    float r, a, vr, va, spin, fire;
    bool dart, rim_checked;
};
struct Pellet {
    float r, a, v;
    bool grazed;
};
struct Shot {
    float r, a;
};
struct Part {
    float x, y, vx, vy, life, max, size;
    uint32_t col;
};
struct Text {
    char s[16];
    float x, y, life;
    uint32_t col;
    bool big;
};
struct Wave {
    float r;
};
struct Queued {
    float at, r, a, vr, va;
    bool dart;
};
struct Star {
    float a, r, sp;
};

template <typename T, int N>
struct Pool {
    T items[N];
    int n = 0;
    T *add()
    {
        return n < N ? &items[n++] : nullptr;
    }
    void remove(int i) { items[i] = items[--n]; }
    void clear() { n = 0; }
};

// ---- the state -----------------------------------------------------------------------

static Effects fx;
static State st = TITLE;
static bool is_paused, marble = true;
static uint32_t best_score, rng = 1;

static float t;  // seconds into the game
static float theta, omega;
static int lives, bombs, combo, mult, kills, grazes;
static uint32_t score;
static bool new_best;
static float invuln, hitstop, shake, flash, combo_t, spawn_t, pulse, over_t;
static uint32_t flash_col;
static bool jumping;
static float jump_t, jump_from, jump_cd;
static bool firing, charging;
static float fire_at, charge;
static bool beam_on;
static float beam_t, beam_a;
static bool prev_fire;
static float shx, shy;  // this frame's shake

static Pool<Enemy, MAX_ENEMIES> enemies;
static Pool<Pellet, MAX_PELLETS> pellets;
static Pool<Shot, MAX_SHOTS> shots;
static Pool<Part, MAX_PARTS> parts;
static Pool<Text, MAX_TEXTS> texts;
static Pool<Wave, MAX_WAVES> waves;
static Pool<Queued, MAX_QUEUE> queue;
static Star stars[STARS];

static int music_level = -1;  // what plays: 0 nothing, 1 calm, 2 hot (once the combo is x3)

// The motor's patterns: on, off, on, ... in ms.
static const uint16_t BUZZ_GRAZE[] = {12, 0};
static const uint16_t BUZZ_TICK[] = {18, 0};
static const uint16_t BUZZ_JUMP[] = {25, 0};
static const uint16_t BUZZ_BEAM[] = {120, 0};
static const uint16_t BUZZ_HIT[] = {90, 50, 90, 0};
static const uint16_t BUZZ_BOMB[] = {160, 60, 90, 0};
static const uint16_t BUZZ_OVER[] = {220, 80, 220, 80, 400, 0};

// ---- helpers -------------------------------------------------------------------------

static float frand()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 0xFFFFFF) / 16777216.0f;
}
static float rand_in(float a, float b) { return a + frand() * (b - a); }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float wrap(float a)
{
    a = fmodf(a + PI, TAU);
    if (a < 0) {
        a += TAU;
    }
    return a - PI;
}
static float X(float r, float a) { return CX + cosf(a) * r + shx; }
static float Y(float r, float a) { return CY + sinf(a) * r + shy; }
static uint32_t fade(uint32_t col, float a) { return gfxu::mix(BLACK, col, a); }

using orbit_sound::Id;

static void sound(Id id)
{
    if (fx.sound) {
        fx.sound(id);
    }
}
static void buzz(const uint16_t *p, uint8_t level)
{
    if (fx.buzz) {
        fx.buzz(p, level);
    }
}

static void say(const char *s, float x, float y, uint32_t col, bool big)
{
    Text *tx = texts.add();
    if (tx == nullptr) {  // full: the oldest goes
        texts.remove(0);
        tx = texts.add();
        if (tx == nullptr) {
            return;
        }
    }
    snprintf(tx->s, sizeof tx->s, "%s", s);
    tx->x = x, tx->y = y, tx->col = col, tx->big = big, tx->life = 0.9f;
}

static void burst(float x, float y, uint32_t col, int n, float speed, float size)
{
    for (int i = 0; i < n; i++) {
        Part *p = parts.add();
        if (p == nullptr) {
            return;
        }
        const float a = rand_in(0, TAU), v = rand_in(0.3f, 1) * speed;
        *p = {x, y, cosf(a) * v, sinf(a) * v, rand_in(0.3f, 0.75f), 0.75f, rand_in(0.6f, 1.2f) * size, col};
    }
}

// ---- the game ------------------------------------------------------------------------

void begin(const Effects &effects, uint32_t best, uint32_t seed)
{
    fx = effects;
    best_score = best;
    rng = seed ? seed : 1;
    st = TITLE;
    is_paused = false;
    music_level = -1;
    pulse = shake = flash = 0;
    mult = 1;
    for (Star &s : stars) {
        s = {rand_in(0, TAU), rand_in(0, 240), rand_in(0.4f, 1.4f)};
    }
}

static void start()
{
    st = PLAY;
    is_paused = false;
    t = 0;
    theta = PI / 2, omega = 0;
    lives = 3, bombs = 2, score = 0, combo = 0, mult = 1, kills = 0, grazes = 0;
    new_best = false;
    invuln = 1.2f, hitstop = 0, shake = 0, flash = 0, combo_t = 0, spawn_t = 1.2f, pulse = 0;
    jumping = false, jump_cd = 0;
    firing = charging = false, charge = 0;
    beam_on = false;
    enemies.clear(), pellets.clear(), shots.clear(), parts.clear(), texts.clear(), waves.clear(), queue.clear();
    sound(orbit_sound::START);
}

static void kill(int i, bool big)
{
    Enemy e = enemies.items[i];
    enemies.remove(i);
    kills++;
    combo++;
    combo_t = COMBO_S;
    const int before = mult;
    mult = combo / 5 + 1 > 8 ? 8 : combo / 5 + 1;
    const uint32_t pts = 100 * mult;
    score += pts;
    pulse = fminf(1.4f, pulse + 0.35f);
    const float x = X(e.r, e.a), y = Y(e.r, e.a);
    burst(x, y, e.dart ? DART : KRAUT, big ? 16 : 10, big ? 240 : 170, 2.4f);
    burst(x, y, 0xFFFFFFu, 3, 90, 1.6f);
    if (e.r > 70) {
        char buf[12];
        snprintf(buf, sizeof buf, "+%u", (unsigned)pts);
        say(buf, x, y - 10, KRAUT, false);
    }
    sound(orbit_sound::KILL);
    if (mult > before) {
        char buf[16];
        snprintf(buf, sizeof buf, "x%d", mult);
        say(buf, CX, CY + 92, AMBER, true);
        sound(orbit_sound::COMBO);
        buzz(BUZZ_TICK, 170);
    }
}

static void hurt()
{
    if (invuln > 0 || jumping) {
        return;
    }
    lives--;
    invuln = 1.5f, hitstop = 0.11f, shake = 16, flash = 0.8f, flash_col = HOT;
    combo = 0, mult = 1, combo_t = 0, charge = 0, charging = false;
    burst(X(R, theta), Y(R, theta), CYAN, 22, 260, 3);
    burst(X(R, theta), Y(R, theta), HOT, 14, 200, 2.6f);
    sound(orbit_sound::HIT);
    buzz(BUZZ_HIT, 255);
    if (lives <= 0) {
        st = OVER;
        over_t = 0;
        if (score > best_score) {
            best_score = score;
            new_best = true;
        }
        sound(orbit_sound::OVER);
        buzz(BUZZ_OVER, 255);
    }
}

static void fire()
{
    if (jumping) {
        return;
    }
    if (Shot *s = shots.add()) {
        *s = {R - 16, theta};
    }
    sound(orbit_sound::SHOOT);
}

static void release_beam()
{
    beam_on = true, beam_t = 0, beam_a = theta;
    sound(orbit_sound::BEAM);
    shake = fmaxf(shake, 10);
    buzz(BUZZ_BEAM, 255);
    for (int i = enemies.n - 1; i >= 0; i--) {
        const Enemy &e = enemies.items[i];
        if (fabsf(wrap(e.a - beam_a)) * fmaxf(e.r, 20) < 22) {
            kill(i, true);
        }
    }
    for (int i = pellets.n - 1; i >= 0; i--) {
        const Pellet &p = pellets.items[i];
        if (fabsf(wrap(p.a - beam_a)) * fmaxf(p.r, 20) < 22) {
            pellets.remove(i);
        }
    }
}

static void bomb()
{
    if (bombs <= 0) {
        return;
    }
    bombs--;
    if (Wave *w = waves.add()) {
        w->r = 10;
    }
    shake = 18, flash = 0.6f, flash_col = AMBER, hitstop = 0.06f;
    sound(orbit_sound::BOMB);
    buzz(BUZZ_BOMB, 255);
    say("KNÖDEL!", CX, CY - 40, AMBER, true);
}

static void jump()
{
    if (jumping || jump_cd > 0) {
        return;
    }
    jumping = true, jump_t = 0, jump_from = theta, jump_cd = JUMP_COOLDOWN;
    sound(orbit_sound::JUMP);
    buzz(BUZZ_JUMP, 160);
}

// Sauerkraut comes out of the vortex in formations; the later, the faster and the more.
static void spawn_wave()
{
    const float d = fminf(1, t / 110), dir = frand() < 0.5f ? -1 : 1, base = rand_in(0, TAU), pick = frand();
    if (pick < 0.45f) {  // a spiral of strands
        const int n = 6 + (int)(5 * d);
        for (int i = 0; i < n; i++) {
            if (Queued *q = queue.add()) {
                *q = {t + i * 0.12f, 6, base + i * 0.3f * dir, 52 + 34 * d, 0.55f * dir, false};
            }
        }
    } else if (pick < 0.8f) {  // a ring opening out
        const int n = 8 + (int)(6 * d);
        for (int i = 0; i < n; i++) {
            if (Queued *q = queue.add()) {
                *q = {t, 8, base + i * TAU / n, 40 + 26 * d, 0.18f * dir, false};
            }
        }
    } else {  // three darts at where you are
        for (int i = -1; i <= 1; i++) {
            if (Queued *q = queue.add()) {
                *q = {t + (i < 0 ? -i : i) * 0.08f, 8, theta + i * 0.22f, 120 + 60 * d, 0, true};
            }
        }
    }
}

// The music follows the game: none on the title, after it and while paused;
// hot once the combo is x3.
static void music()
{
    const int level = st != PLAY || is_paused ? 0 : mult >= 3 ? 2 : 1;
    if (level != music_level) {
        music_level = level;
        if (fx.music) {
            fx.music(level);
        }
    }
}

static void ambient(float dt)
{
    pulse = fmaxf(0, pulse - dt * 1.6f);
    const float heat = st == PLAY ? mult : 1;
    for (Star &s : stars) {
        s.r += s.sp * (40 + s.r * 1.1f) * (0.6f + heat * 0.25f) * dt;
        if (s.r > 250) {
            s.r = rand_in(0, 18);
            s.a = rand_in(0, TAU);
        }
    }
    for (int i = parts.n - 1; i >= 0; i--) {
        Part &p = parts.items[i];
        p.x += p.vx * dt, p.y += p.vy * dt;
        p.vx *= 1 - 2.2f * dt, p.vy *= 1 - 2.2f * dt;
        p.life -= dt;
        if (p.life <= 0) {
            parts.remove(i);
        }
    }
    for (int i = texts.n - 1; i >= 0; i--) {
        texts.items[i].y -= 28 * dt;
        texts.items[i].life -= dt;
        if (texts.items[i].life <= 0) {
            texts.remove(i);
        }
    }
    shake = fmaxf(0, shake - dt * 50);
    flash = fmaxf(0, flash - dt * 2.2f);
}

void update(float dt, const Input &in)
{
    const bool pressed = in.fire && !prev_fire, released = !in.fire && prev_fire;
    prev_fire = in.fire;
    ambient(dt);
    music();

    if (st != PLAY) {
        over_t += dt;
        if (pressed && (st == TITLE || over_t > 1.0f)) {
            start();
        }
        return;
    }
    if (is_paused) {
        if (pressed) {
            is_paused = false;
        }
        return;
    }
    if (hitstop > 0) {
        hitstop -= dt;
        return;
    }
    t += dt;
    invuln = fmaxf(0, invuln - dt);
    jump_cd = fmaxf(0, jump_cd - dt);
    combo_t -= dt;
    if (combo_t <= 0 && combo > 0) {
        combo = 0, mult = 1;
    }

    // The keys.
    if (pressed) {
        firing = true, fire_at = t;
        fire();
    }
    if (released && firing) {
        firing = false;
        if (charge >= 1) {
            release_beam();
        }
        charge = 0, charging = false;
    }
    if (firing && !jumping && t - fire_at > 0.22f) {
        const float before = charge;
        charging = true;
        charge = fminf(1, charge + dt / CHARGE_S);
        if ((int)(before * 6) != (int)(charge * 6) && charge < 1) {
            sound(orbit_sound::TICK);
        }
        if (before < 1 && charge >= 1) {
            sound(orbit_sound::CHARGED);
            buzz(BUZZ_GRAZE, 140);
        }
    }
    if (in.jump) {
        jump();
    }
    if (in.bomb) {
        bomb();
    }

    // The ship: a marble on the rim (rolls toward the lean), or steered like a wheel.
    float tx = in.tilt_x, ty = in.tilt_y;
    const float m = sqrtf(tx * tx + ty * ty);
    if (m < 0.08f) {
        tx = ty = 0;
    } else if (m > 1) {
        tx /= m, ty /= m;
    }
    if (marble) {
        const float tang = -tx * sinf(theta) + ty * cosf(theta);
        omega += (tang * 17 - omega * 4.2f) * dt;
    } else {
        omega += (tx * 4.4f - omega) * fminf(1, 10 * dt);
    }
    omega = clampf(omega, -5, 5);
    if (!jumping) {
        theta = wrap(theta + omega * dt);
    }

    // The jump: straight through the middle to the other side, taking what is in the way.
    if (jumping) {
        jump_t += dt;
        const float u = fminf(1, jump_t / JUMP_S), s = 1 - 2 * u;
        const float x = CX + cosf(jump_from) * R * s, y = CY + sinf(jump_from) * R * s;
        if (Part *p = parts.add()) {
            *p = {x, y, 0, 0, 0.3f, 0.3f, 5, CYAN};
        }
        for (int i = enemies.n - 1; i >= 0; i--) {
            const Enemy &e = enemies.items[i];
            if (hypotf(CX + cosf(e.a) * e.r - x, CY + sinf(e.a) * e.r - y) < 20) {
                kill(i, true);
            }
        }
        if (u >= 1) {
            theta = wrap(jump_from + PI);
            jumping = false;
            invuln = fmaxf(invuln, 0.25f);
        }
    }
    if (beam_on && (beam_t += dt) > BEAM_S) {
        beam_on = false;
    }

    // Waves.
    const float d = fminf(1, t / 110);
    spawn_t -= dt;
    if (spawn_t <= 0) {
        spawn_wave();
        spawn_t = 2.5f - 1.45f * d + rand_in(-0.3f, 0.3f);
    }
    for (int i = queue.n - 1; i >= 0; i--) {
        const Queued q = queue.items[i];
        if (q.at <= t) {
            queue.remove(i);
            if (Enemy *e = enemies.add()) {
                *e = {q.r, q.a, q.vr, q.va, rand_in(0, TAU), rand_in(0.8f, 2.6f), q.dart, false};
            }
        }
    }

    for (int i = enemies.n - 1; i >= 0; i--) {
        Enemy &e = enemies.items[i];
        e.r += e.vr * dt * (0.55f + e.r / R * 0.8f);
        e.a = wrap(e.a + e.va * dt);
        e.spin += dt * 3;
        if (!e.dart && e.r > 30 && e.r < R - 70) {
            e.fire -= dt;
            if (e.fire <= 0) {
                if (Pellet *p = pellets.add()) {
                    *p = {e.r, e.a, 150 + 110 * d, false};
                }
                e.fire = rand_in(1.6f, 3.4f) * (1 - 0.45f * d);
            }
        }
        if (e.r > R - 10 && !e.rim_checked) {
            e.rim_checked = true;
            if (!jumping && fabsf(wrap(e.a - theta)) * R < 19) {
                enemies.remove(i);
                hurt();
                continue;
            }
        }
        if (e.r > R + 34) {
            enemies.remove(i);
        }
    }

    for (int i = pellets.n - 1; i >= 0; i--) {
        Pellet &p = pellets.items[i];
        p.r += p.v * dt;
        if (p.r > R - 9 && p.r < R + 9 && !jumping) {
            const float arc = fabsf(wrap(p.a - theta)) * R;
            if (arc < 12) {
                pellets.remove(i);
                hurt();
                continue;
            }
            if (arc < 34 && !p.grazed && invuln <= 0) {
                p.grazed = true;
                grazes++;
                score += 20 * mult;
                combo_t = fmaxf(combo_t, 1.2f);
                burst(X(R, p.a), Y(R, p.a), 0xFFFFFFu, 5, 120, 1.4f);
                sound(orbit_sound::GRAZE);
                buzz(BUZZ_GRAZE, 150);
            }
        }
        if (p.r > R + 40) {
            pellets.remove(i);
        }
    }

    for (int i = shots.n - 1; i >= 0; i--) {
        Shot &s = shots.items[i];
        s.r -= 620 * dt;
        bool gone = s.r < 6;
        const float x = CX + cosf(s.a) * s.r, y = CY + sinf(s.a) * s.r;
        for (int k = enemies.n - 1; k >= 0 && !gone; k--) {
            const Enemy &e = enemies.items[k];
            const float size = 6 + 10 * e.r / R;
            if (hypotf(CX + cosf(e.a) * e.r - x, CY + sinf(e.a) * e.r - y) < size + 5) {
                kill(k, false);
                gone = true;
            }
        }
        if (gone) {
            shots.remove(i);
        }
    }

    for (int i = waves.n - 1; i >= 0; i--) {
        Wave &w = waves.items[i];
        w.r += 520 * dt;
        for (int k = enemies.n - 1; k >= 0; k--) {
            if (enemies.items[k].r < w.r) {
                kill(k, true);
            }
        }
        for (int k = pellets.n - 1; k >= 0; k--) {
            if (pellets.items[k].r < w.r) {
                const Pellet &p = pellets.items[k];
                burst(CX + cosf(p.a) * p.r, CY + sinf(p.a) * p.r, AMBER, 2, 80, 1.4f);
                pellets.remove(k);
            }
        }
        if (w.r > 280) {
            waves.remove(i);
        }
    }
}

State state() { return st; }
bool paused() { return is_paused; }
void set_paused(bool on) { is_paused = on && st == PLAY; }
void set_marble(bool on) { marble = on; }
uint32_t best() { return best_score; }

void counts(int &e, int &p, int &pa)
{
    e = enemies.n;
    p = pellets.n;
    pa = parts.n;
}

// ---- drawing -------------------------------------------------------------------------

static uint64_t (*clock_us)();
static uint32_t spent[PROFILE_PARTS];
static uint64_t mark_us;

void set_clock(uint64_t (*micros)()) { clock_us = micros; }

void profile(uint32_t us[PROFILE_PARTS])
{
    for (int i = 0; i < PROFILE_PARTS; i++) {
        us[i] = spent[i];
        spent[i] = 0;
    }
}

// The time since the last mark goes to `part`.
static void mark(int part)
{
    if (clock_us) {
        const uint64_t now = clock_us();
        if (part >= 0) {
            spent[part] += (uint32_t)(now - mark_us);
        }
        mark_us = now;
    }
}

static void text(G *g, const lgfx::IFont *f, const char *s, float x, float y, uint32_t col, float size = 1)
{
    g->setFont(f);
    g->setTextSize(size);
    g->setTextDatum(middle_center);
    g->setTextColor(col);
    g->drawString(s, (int)x, (int)y);
    g->setTextSize(1);
}

// A line with a soft halo: a wide dim stroke under a thin bright one.
static void glow_line(G *g, float x0, float y0, float x1, float y1, float w, uint32_t col, float a = 1)
{
    gfxu::line(g, x0, y0, x1, y1, w * 3, fade(col, 0.2f * a));
    gfxu::line(g, x0, y0, x1, y1, w, fade(col, a));
}

static void glow_dot(G *g, float x, float y, float r, uint32_t col)
{
    g->fillCircle((int)x, (int)y, (int)(r * 2.4f), fade(col, 0.16f));
    g->fillCircle((int)x, (int)y, (int)fmaxf(1, r), col);
}

static void draw_stars(G *g)
{
    const float heat = st == PLAY ? 0.6f + mult * 0.3f : 0.8f;
    for (const Star &s : stars) {
        const float len = 2 + s.r * 0.06f * heat, a = fminf(1, s.r / 160) * 0.6f;
        g->drawLine((int)X(s.r - len, s.a), (int)Y(s.r - len, s.a), (int)X(s.r, s.a), (int)Y(s.r, s.a), fade(STAR, a));
    }
}

static void draw_vortex(G *g, float clock, float cx, float cy, float k)
{
    const float heat = st == PLAY ? clampf((mult - 1) / 5.0f, 0, 1) : 0;
    const float rot = clock * (0.7f + heat * 1.4f);
    const uint32_t arm = gfxu::mix(VIOLET, HOT, heat);
    for (int a = 0; a < 5; a++) {
        float px = 0, py = 0;
        for (int r = 4; r <= 74; r += 5) {
            const float ang = rot + a * TAU / 5 + r * 0.045f;
            const float x = cx + cosf(ang) * r * k, y = cy + sinf(ang) * r * k;
            if (r > 4) {
                const uint32_t col = fade(arm, (0.85f - r / 110.0f) * (0.7f + pulse * 0.3f));
                g->drawLine((int)px, (int)py, (int)x, (int)y, col);
                g->drawLine((int)px + 1, (int)py, (int)x + 1, (int)y, col);
            }
            px = x, py = y;
        }
    }
    const float core = (9 + pulse * 9 + sinf(clock * 6) * 1.2f) * k;
    const uint32_t mid = heat > 0.5f ? HOT : VIOLET;
    g->fillCircle((int)cx, (int)cy, (int)(core * 2.2f), fade(mid, 0.22f));
    g->fillCircle((int)cx, (int)cy, (int)(core * 1.4f), fade(mid, 0.75f));
    g->fillCircle((int)cx, (int)cy, (int)(core * 0.8f), 0xFFFFFFu);
}

// A Sauerkraut tangle: three wavy strands, bigger as it comes closer.
static void draw_kraut(G *g, const Enemy &e)
{
    const float x = X(e.r, e.a), y = Y(e.r, e.a), s = 5 + 11 * fminf(1.1f, e.r / R);
    const uint32_t col = e.dart ? DART : KRAUT;
    g->fillCircle((int)x, (int)y, (int)(s * 1.15f), fade(col, 0.13f));
    for (int k = 0; k < 3; k++) {
        const float rot = e.spin + k * TAU / 3, c = cosf(rot), sn = sinf(rot);
        float px = 0, py = 0;
        for (int i = 0; i <= 6; i++) {
            const float u = i / 6.0f, lx = (u - 0.5f) * s * 2, ly = sinf(u * 9 + e.spin * 2 + k) * s * 0.28f;
            const float qx = x + lx * c - ly * sn, qy = y + lx * sn + ly * c;
            if (i > 0) {
                g->drawLine((int)px, (int)py, (int)qx, (int)qy, col);
                if (s > 9) {
                    g->drawLine((int)px, (int)py + 1, (int)qx, (int)qy + 1, col);
                }
            }
            px = qx, py = qy;
        }
    }
}

static void draw_ship(G *g, float x, float y, float a, bool dim, float clock)
{
    const float c = cosf(a + PI), s = sinf(a + PI);  // the nose points at the vortex
    auto P = [&](float lx, float ly, float &ox, float &oy) {
        ox = x + lx * c - ly * s;
        oy = y + lx * s + ly * c;
    };
    float nx, ny, lx, ly, mx, my, rx, ry, f0x, f0y, f1x, f1y, f2x, f2y;
    P(14, 0, nx, ny), P(-9, -11, lx, ly), P(-4, 0, mx, my), P(-9, 11, rx, ry);
    const float flame = 7 + 6 * fabsf(sinf(clock * 37));
    P(-7, -4, f0x, f0y), P(-7 - flame, 0, f1x, f1y), P(-7, 4, f2x, f2y);
    const uint32_t edge = dim ? fade(CYAN, 0.35f) : CYAN;
    g->fillTriangle((int)f0x, (int)f0y, (int)f1x, (int)f1y, (int)f2x, (int)f2y, dim ? fade(AMBER, 0.35f) : AMBER);
    g->fillCircle((int)x, (int)y, 21, fade(CYAN, dim ? 0.06f : 0.13f));
    g->fillTriangle((int)nx, (int)ny, (int)lx, (int)ly, (int)mx, (int)my, 0x0B1A20u);
    g->fillTriangle((int)nx, (int)ny, (int)rx, (int)ry, (int)mx, (int)my, 0x0B1A20u);
    gfxu::line(g, nx, ny, lx, ly, 2.4f, edge);
    gfxu::line(g, lx, ly, mx, my, 2.4f, edge);
    gfxu::line(g, mx, my, rx, ry, 2.4f, edge);
    gfxu::line(g, rx, ry, nx, ny, 2.4f, edge);
}

// A ring around (x, y) filled clockwise from the top, `u` of the way.
static void progress_ring(G *g, float x, float y, int r, float u, uint32_t col)
{
    if (u > 0.01f) {
        g->fillArc((int)x, (int)y, r, r - 2, -90, -90 + 360 * fminf(1, u), col);
    }
}

// The stars and the vortex; with `full`, all that is in play too. (The title,
// the end and a pause show only the stars and the vortex: a dark veil over the
// whole screen took over 100 ms a frame.)
static void draw_world(G *g, float clock, bool full)
{
    draw_stars(g);
    draw_vortex(g, clock, CX + shx, CY + shy, 1);
    mark(0);
    if (!full) {
        return;
    }
    g->drawCircle((int)(CX + shx), (int)(CY + shy), (int)R, fade(CYAN, 0.13f));
    // A tick on the rim where something is about to arrive.
    for (int i = 0; i < pellets.n; i++) {
        const Pellet &p = pellets.items[i];
        if (p.r > R - 80) {
            gfxu::line(g, X(R + 14, p.a), Y(R + 14, p.a), X(R + 24, p.a), Y(R + 24, p.a), 2.6f,
                       fade(HOT, clampf((p.r - (R - 80)) / 80, 0, 1)));
        }
    }
    for (int i = 0; i < enemies.n; i++) {
        const Enemy &e = enemies.items[i];
        if (e.r > R - 70 && e.r < R) {
            gfxu::line(g, X(R + 12, e.a), Y(R + 12, e.a), X(R + 26, e.a), Y(R + 26, e.a), 3.4f,
                       fade(DART, clampf((e.r - (R - 70)) / 70, 0, 1)));
        }
    }
    for (int i = 0; i < waves.n; i++) {
        const Wave &w = waves.items[i];
        const uint32_t col = fade(AMBER, clampf(1 - w.r / 280, 0, 1));
        for (int k = 0; k < 12; k += 2) {
            g->drawCircle((int)(CX + shx), (int)(CY + shy), (int)(w.r - k), col);
        }
    }
    for (int i = 0; i < enemies.n; i++) {
        draw_kraut(g, enemies.items[i]);
    }
    for (int i = 0; i < pellets.n; i++) {
        const Pellet &p = pellets.items[i];
        glow_dot(g, X(p.r, p.a), Y(p.r, p.a), 2.2f + 2.4f * p.r / R, PELLET);
    }
    for (int i = 0; i < shots.n; i++) {
        const Shot &s = shots.items[i];
        const int x0 = (int)X(s.r + 16, s.a), y0 = (int)Y(s.r + 16, s.a), x1 = (int)X(s.r, s.a), y1 = (int)Y(s.r, s.a);
        g->drawLine(x0 - 1, y0, x1 - 1, y1, fade(CYAN, 0.45f));
        g->drawLine(x0 + 1, y0, x1 + 1, y1, fade(CYAN, 0.45f));
        g->drawLine(x0, y0 - 1, x1, y1 - 1, CYAN);
        g->drawLine(x0, y0, x1, y1, 0xFFFFFFu);
    }
    if (beam_on) {
        const float u = beam_t / BEAM_S, w = 26 * (1 - u) + 2;
        glow_line(g, X(R, beam_a), Y(R, beam_a), CX + shx, CY + shy, w, AMBER, 1 - u * 0.6f);
        gfxu::line(g, X(R, beam_a), Y(R, beam_a), CX + shx, CY + shy, w * 0.35f, fade(0xFFFFFFu, 1 - u));
    }
    for (int i = 0; i < parts.n; i++) {
        const Part &p = parts.items[i];
        const uint32_t col = fade(p.col, clampf(p.life / p.max, 0, 1));
        const int sz = (int)(p.size + 0.5f);
        if (sz <= 1) {
            g->fillRect((int)(p.x + shx), (int)(p.y + shy), 2, 2, col);
        } else {
            g->fillCircle((int)(p.x + shx), (int)(p.y + shy), sz, col);
        }
    }
    mark(1);
    if (st == PLAY) {
        float x = X(R, theta), y = Y(R, theta), a = theta;
        if (jumping) {
            const float u = fminf(1, jump_t / JUMP_S), s = 1 - 2 * u;
            x = CX + shx + cosf(jump_from) * R * s, y = CY + shy + sinf(jump_from) * R * s;
            a = s >= 0 ? jump_from : jump_from + PI;
        }
        draw_ship(g, x, y, a, invuln > 0 && ((int)(clock * 16) & 1), clock);
        if (charging) {
            const bool full = charge >= 1;
            progress_ring(g, x, y, 25, charge, full ? fade(AMBER, 0.6f + 0.4f * sinf(clock * 30)) : AMBER);
        }
        if (jump_cd > 0 && !jumping) {
            progress_ring(g, x, y, 31, 1 - jump_cd / JUMP_COOLDOWN, fade(CYAN, 0.35f));
        }
    }
    for (int i = 0; i < texts.n; i++) {
        const Text &tx = texts.items[i];
        text(g, ui::font(tx.big ? ui::FontId::DE_MB : ui::FontId::DE_S), tx.s, tx.x, tx.y,
             fade(tx.col, clampf(tx.life / 0.5f, 0, 1)));
    }
}

static void draw_hud(G *g, const UiText &tx)
{
    char buf[24];
    snprintf(buf, sizeof buf, "%06u", (unsigned)score);
    text(g, ui::font(ui::FontId::DE_M), buf, CX, 46, INK);
    for (int i = 0; i < 3; i++) {
        const float x = CX - 22 + i * 22, y = 74;
        const uint32_t col = i < lives ? CYAN : 0x2A2833u;
        g->fillTriangle((int)x, (int)(y - 7), (int)(x - 6), (int)(y + 5), (int)x, (int)(y + 2), col);
        g->fillTriangle((int)x, (int)(y - 7), (int)(x + 6), (int)(y + 5), (int)x, (int)(y + 2), col);
    }
    for (int i = 0; i < 2; i++) {
        if (i < bombs) {
            glow_dot(g, CX - 9 + i * 18, 442, 4.5f, AMBER);
        } else {
            g->fillCircle((int)(CX - 9 + i * 18), 442, 4, 0x3A3646u);
        }
    }
    if (mult > 1) {
        snprintf(buf, sizeof buf, "x%d", mult);
        text(g, ui::font(ui::FontId::DE_M), buf, CX, 396, AMBER);
        progress_ring(g, CX, 396, 21, combo_t / COMBO_S, fade(AMBER, 0.8f));
    }
    if (is_paused) {
        text(g, ui::font(ui::FontId::DE_MB), "PAUSE", CX, CY - 8, INK);
        text(g, ui::font(ui::FontId::LOC_S), tx.game_paused, CX, CY + 34, MUTED);
    }
}

// Flashing: the edge of the screen lights up in the colour of what happened.
static void draw_flash(G *g)
{
    if (flash <= 0.02f) {
        return;
    }
    for (int i = 0; i < 16; i++) {
        g->drawCircle((int)CX, (int)CY, 233 - i, fade(flash_col, flash * (1 - i / 16.0f)));
    }
}

static void draw_title(G *g, float clock, const UiText &tx)
{
    g->fillCircle((int)CX, (int)CY, 150, fade(0x161226u, 0.8f));
    text(g, ui::font(ui::FontId::DE_MB), "KNÖDEL", CX, CY - 56, CYAN, 1.6f);
    text(g, ui::font(ui::FontId::DE_MB), "ORBIT", CX, CY - 6, HOT, 1.6f);
    text(g, ui::font(ui::FontId::LOC_S), tx.game_start, CX, CY + 50, fade(INK, 0.6f + 0.4f * sinf(clock * 4)));
    text(g, ui::font(ui::FontId::LOC_S), marble ? tx.game_marble : tx.game_wheel, CX, CY + 80, CYAN);
    if (best_score > 0) {
        char buf[48];
        snprintf(buf, sizeof buf, tx.game_best, (unsigned)best_score);
        text(g, ui::font(ui::FontId::LOC_S), buf, CX, CY + 108, MUTED);
    }
    const float a = PI / 2 + sinf(clock) * 0.5f;
    draw_ship(g, X(R, a), Y(R, a), a, false, clock);
}

static void draw_over(G *g, float clock, const UiText &tx)
{
    char buf[64];
    text(g, ui::font(ui::FontId::DE_MB), "SCHLUSS!", CX, CY - 76, HOT, 1.3f);
    snprintf(buf, sizeof buf, "%u", (unsigned)score);
    text(g, ui::font(ui::FontId::CLOCK), buf, CX, CY - 12, INK);
    snprintf(buf, sizeof buf, tx.game_stats, kills, grazes, (int)t);
    text(g, ui::font(ui::FontId::LOC_S), buf, CX, CY + 40, MUTED);
    if (new_best) {
        text(g, ui::font(ui::FontId::LOC_S), tx.game_new_best, CX, CY + 70, AMBER);
    } else {
        snprintf(buf, sizeof buf, tx.game_best, (unsigned)best_score);
        text(g, ui::font(ui::FontId::LOC_S), buf, CX, CY + 70, MUTED);
    }
    if (over_t > 1) {
        text(g, ui::font(ui::FontId::LOC_S), tx.game_again, CX, CY + 108, fade(INK, 0.6f + 0.4f * sinf(clock * 4)));
    }
}

void draw(G *g, float clock, const UiText &tx)
{
    mark(-1);
    g->fillScreen(BLACK);
    shx = shy = 0;
    if (shake > 0) {
        shx = rand_in(-shake, shake) * 0.6f;
        shy = rand_in(-shake, shake) * 0.6f;
    }
    draw_world(g, clock, st == PLAY && !is_paused);
    mark(2);
    shx = shy = 0;
    draw_flash(g);
    if (st == TITLE) {
        draw_title(g, clock, tx);
    } else if (st == PLAY) {
        draw_hud(g, tx);
    } else {
        draw_over(g, clock, tx);
    }
    mark(3);
}

void draw_icon(G *g, float cx, float cy, float r, float clock)
{
    g->fillCircle((int)cx, (int)cy, (int)r, 0x07060Bu);
    const float k = r / 150.0f;
    // A few stars streaming out.
    for (int i = 0; i < 14; i++) {
        const float a = i * 2.39996f, rr = fmodf(clock * (30 + i * 3) + i * 37, 120) * k + 20 * k;
        const float len = 3 + rr * 0.08f;
        g->drawLine((int)(cx + cosf(a) * (rr - len)), (int)(cy + sinf(a) * (rr - len)), (int)(cx + cosf(a) * rr),
                    (int)(cy + sinf(a) * rr), fade(STAR, fminf(1, rr / (100 * k)) * 0.7f));
    }
    const float saved = pulse;
    pulse = 0.4f + 0.3f * sinf(clock * 3);
    draw_vortex(g, clock, cx, cy, k * 1.1f);
    pulse = saved;
    g->drawCircle((int)cx, (int)cy, (int)(112 * k), fade(CYAN, 0.18f));
    const float a = clock * 1.3f;
    const float sx = shx, sy = shy;
    shx = shy = 0;
    draw_ship(g, cx + cosf(a) * 112 * k, cy + sinf(a) * 112 * k, a, false, clock);
    shx = sx, shy = sy;
}

}  // namespace orbit
