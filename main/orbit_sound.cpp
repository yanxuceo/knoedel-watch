#include "orbit_sound.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <stdlib.h>

#include <vector>

namespace orbit_sound {

namespace {

constexpr float PI = 3.14159265f;
Clip clips[COUNT];
void (*pause_now)() = nullptr;  // called now and then while making, to let other work run
long since_pause = 0;

void maybe_pause()
{
    if (pause_now && ++since_pause >= 1500) {
        since_pause = 0;
        pause_now();
    }
}

// A clip being made: floats, mixed into; a loop wraps what runs past its end.
struct Buf {
    std::vector<float> s;
    bool loop = false;
    explicit Buf(float seconds, bool looped = false) : s((size_t)(seconds * RATE), 0.0f), loop(looped) {}
    void add(long i, float v)
    {
        if (i < 0) {
            return;
        }
        if ((size_t)i >= s.size()) {
            if (!loop) {
                return;
            }
            i %= (long)s.size();
        }
        s[(size_t)i] += v;
    }
};

enum Wave { SINE, TRI, SAW, SOFTSQ };

float wave(Wave w, float ph)  // ph in turns
{
    ph -= floorf(ph);
    switch (w) {
    case SINE: return sinf(2 * PI * ph);
    case TRI: return 4 * fabsf(ph - 0.5f) - 1;
    case SAW: return 2 * ph - 1;
    case SOFTSQ: return tanhf(3.0f * sinf(2 * PI * ph));
    }
    return 0;
}

// The volume over time: up in `attack` seconds, then dying away with time
// constant `tau`, and to nothing in the last 5 ms. Stepped sample by sample
// (no exp() each time: on the watch that made the sounds take ten seconds).
struct Env {
    long attack_n, n, i = 0;
    float e = 0, decay;
    Env(float attack, float tau, float dur)
        : attack_n((long)(attack * RATE) + 1), n((long)(dur * RATE)), decay(expf(-1.0f / (tau * RATE)))
    {
    }
    float next()
    {
        e = i < attack_n ? (float)i / attack_n : e * decay;
        const long tail = n - i++;
        return tail < (long)(0.005f * RATE) ? e * tail / (0.005f * RATE) : e;
    }
};

// A tone gliding from f0 to f1 Hz (exponentially) over `dur` seconds.
void tone(Buf &b, float at, float dur, float f0, float f1, Wave w, float amp, float attack, float tau)
{
    const long start = (long)(at * RATE), n = (long)(dur * RATE);
    const float k = powf(f1 / f0, 1.0f / n);
    float ph = 0, f = f0;
    Env e(attack, tau, dur);
    for (long i = 0; i < n; i++) {
        ph += f / RATE;
        f *= k;
        b.add(start + i, wave(w, ph) * amp * e.next());
        maybe_pause();
    }
}

// A bell (two-operator FM): bright at first, mellow as it fades.
void bell(Buf &b, float at, float f, float amp, float tau, float dur = 0)
{
    dur = dur > 0 ? dur : tau * 5;
    const long start = (long)(at * RATE), n = (long)(dur * RATE);
    const float index_decay = expf(-1.0f / (tau * 0.6f * RATE));
    float index = 2.2f, ph = 0;
    Env e(0.002f, tau, dur);
    for (long i = 0; i < n; i++) {
        ph += f / RATE;
        ph -= floorf(ph);
        const float v = sinf(2 * PI * ph + index * sinf(4 * PI * ph));
        index *= index_decay;
        b.add(start + i, v * amp * e.next());
        maybe_pause();
    }
}

uint32_t seed = 22222;
float white()
{
    seed = seed * 1664525u + 1013904223u;
    return ((seed >> 9) & 0x7FFF) / 16384.0f - 1.0f;
}

// Noise through a low-pass gliding from lp0 to lp1 Hz, and a high-pass at hp Hz (0: none).
void noise(Buf &b, float at, float dur, float amp, float attack, float tau, float lp0, float lp1, float hp)
{
    const long start = (long)(at * RATE), n = (long)(dur * RATE);
    float low = 0, hp_prev_in = 0, hp_out = 0, lp = fminf(lp0, RATE * 0.45f);
    const float k = powf(fminf(lp1, RATE * 0.45f) / lp, 1.0f / n);
    const float hp_a = hp > 0 ? 1.0f / (1.0f + 2 * PI * hp / RATE) : 0;
    Env e(attack, tau, dur);
    for (long i = 0; i < n; i++) {
        const float w = 2 * PI * lp / RATE, a = w / (1 + w);  // one-pole low-pass, near enough
        lp *= k;
        low += (white() - low) * a;
        float v = low;
        if (hp > 0) {
            hp_out = hp_a * (hp_out + v - hp_prev_in);
            hp_prev_in = v;
            v = hp_out;
        }
        b.add(start + i, v * amp * e.next());
        maybe_pause();
    }
}

bool keep(Id id, const Buf &b, float gain, float level = 1)
{
    // In PSRAM, small ones too: in internal RAM the short effects (under 16 KB
    // each, 45 KB in all) left too little for Wi-Fi to send the memos.
    int16_t *pcm = (int16_t *)heap_caps_malloc(b.s.size() * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (pcm == nullptr) {
        return false;
    }
    for (size_t i = 0; i < b.s.size(); i++) {
        pcm[i] = (int16_t)(tanhf(b.s[i] * gain) * 30000.0f * level);  // a soft limit, never a hard clip
    }
    clips[id] = {pcm, b.s.size()};
    return true;
}

// The music: 128 bpm, two bars of eighths, a plucked bass in A minor and a soft
// kick; hot, with hi-hats and a bell arpeggio over it.
bool music(Id id, bool hot)
{
    const float step = 60.0f / 128 / 2;
    Buf b(step * 16, true);
    static const float BASS[16] = {220, 220, 262, 220, 196, 196, 165, 196, 220, 220, 262, 294, 330, 294, 262, 196};
    for (int i = 0; i < 16; i++) {
        const float at = i * step, f = BASS[i];
        tone(b, at, step * 0.95f, f, f, TRI, 0.30f, 0.004f, 0.11f);
        tone(b, at, step * 0.6f, f * 2, f * 2, SINE, 0.10f, 0.003f, 0.06f);
        if (i % 2 == 0) {  // the kick: a falling thump with a click
            tone(b, at, 0.16f, 190, 60, SINE, 0.55f, 0.001f, 0.06f);
            tone(b, at, 0.10f, 140, 70, SOFTSQ, 0.14f, 0.001f, 0.03f);
            noise(b, at, 0.012f, 0.25f, 0.0005f, 0.004f, 5000, 3000, 0);
        }
        if (hot) {
            if (i % 2 == 1) {
                noise(b, at, 0.05f, 0.30f, 0.001f, 0.012f, 9000, 9000, 4500);
            }
            static const float ARP[4] = {880, 1047, 1319, 1047};
            bell(b, at, ARP[i % 4] * (i >= 8 ? 1.122f : 1.0f), 0.07f, 0.07f, step);
        }
        if (i == 4 || i == 12) {  // a snare-ish clap on the off-beat of each bar
            noise(b, at, 0.14f, hot ? 0.32f : 0.22f, 0.001f, 0.04f, 4500, 1800, 900);
        }
    }
    return keep(id, b, 1.2f, 0.5f);  // under the effects
}

}  // namespace

bool make(void (*pause)())
{
    if (clips[COUNT - 1].pcm != nullptr) {
        return true;
    }
    pause_now = pause;
    {  // pew: a falling sine with a glassy overtone
        Buf b(0.11f);
        tone(b, 0, 0.11f, 1700, 520, SINE, 0.5f, 0.002f, 0.04f);
        tone(b, 0, 0.07f, 3400, 1040, TRI, 0.12f, 0.001f, 0.025f);
        if (!keep(SHOOT, b, 1.0f)) return false;
    }
    {  // a crunchy burst, and the thing falling apart
        Buf b(0.24f);
        noise(b, 0, 0.24f, 0.7f, 0.001f, 0.055f, 6000, 700, 250);
        tone(b, 0, 0.12f, 620, 170, SINE, 0.4f, 0.001f, 0.045f);
        if (!keep(KILL, b, 1.2f)) return false;
    }
    {  // a bright ping
        Buf b(0.12f);
        bell(b, 0, 2637, 0.32f, 0.03f, 0.12f);
        tone(b, 0, 0.05f, 3951, 3951, SINE, 0.08f, 0.001f, 0.015f);
        if (!keep(GRAZE, b, 1.0f)) return false;
    }
    {  // hit: a heavy thump with grit
        Buf b(0.55f);
        noise(b, 0, 0.55f, 0.9f, 0.001f, 0.12f, 3200, 220, 120);
        tone(b, 0, 0.45f, 240, 60, SOFTSQ, 0.55f, 0.001f, 0.14f);
        tone(b, 0, 0.30f, 480, 120, SAW, 0.18f, 0.001f, 0.08f);
        if (!keep(HIT, b, 1.6f)) return false;
    }
    {  // jump: a whoosh rising through the middle
        Buf b(0.28f);
        noise(b, 0, 0.28f, 0.5f, 0.06f, 0.12f, 700, 7000, 350);
        tone(b, 0, 0.24f, 320, 1500, SINE, 0.25f, 0.03f, 0.09f);
        if (!keep(JUMP, b, 1.1f)) return false;
    }
    {  // beam: a roaring sweep down
        Buf b(0.65f);
        tone(b, 0, 0.65f, 1200, 140, SAW, 0.30f, 0.004f, 0.25f);
        tone(b, 0, 0.55f, 600, 70, SOFTSQ, 0.30f, 0.004f, 0.22f);
        noise(b, 0, 0.5f, 0.45f, 0.002f, 0.15f, 9000, 600, 200);
        bell(b, 0, 1760, 0.15f, 0.08f, 0.3f);
        if (!keep(BEAM, b, 1.4f)) return false;
    }
    {  // bomb: a boom that rolls on
        Buf b(1.2f);
        noise(b, 0, 1.2f, 1.0f, 0.002f, 0.32f, 2600, 120, 60);
        tone(b, 0, 1.0f, 200, 45, SOFTSQ, 0.6f, 0.002f, 0.35f);
        noise(b, 0.05f, 0.4f, 0.4f, 0.001f, 0.08f, 7000, 1500, 600);
        if (!keep(BOMB, b, 1.8f)) return false;
    }
    {  // combo up: two bells, a fifth apart
        Buf b(0.32f);
        bell(b, 0, 1319, 0.3f, 0.06f, 0.2f);
        bell(b, 0.08f, 1976, 0.3f, 0.08f, 0.24f);
        if (!keep(COMBO, b, 1.0f)) return false;
    }
    {  // charging: a soft tick
        Buf b(0.04f);
        tone(b, 0, 0.04f, 1200, 1300, SINE, 0.18f, 0.001f, 0.012f);
        if (!keep(TICK, b, 1.0f)) return false;
    }
    {  // charged
        Buf b(0.3f);
        bell(b, 0, 1760, 0.3f, 0.06f, 0.25f);
        bell(b, 0.05f, 2637, 0.2f, 0.06f, 0.25f);
        if (!keep(CHARGED, b, 1.0f)) return false;
    }
    {  // start: up an A major chord
        Buf b(0.6f);
        const float notes[4] = {440, 554, 659, 880};
        for (int i = 0; i < 4; i++) bell(b, i * 0.08f, notes[i], 0.28f, 0.09f, 0.6f - i * 0.08f);
        if (!keep(START, b, 1.0f)) return false;
    }
    {  // game over: down, slowly
        Buf b(1.4f);
        const float notes[4] = {523, 392, 330, 262};
        for (int i = 0; i < 4; i++) bell(b, i * 0.22f, notes[i], 0.34f, i == 3 ? 0.3f : 0.15f, 1.4f - i * 0.22f);
        if (!keep(OVER, b, 1.0f)) return false;
    }
    return music(MUSIC_CALM, false) && music(MUSIC_HOT, true);
}

Clip clip(Id id) { return id < COUNT ? clips[id] : Clip{nullptr, 0}; }

}  // namespace orbit_sound
