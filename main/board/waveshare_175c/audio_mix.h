#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ws {
constexpr uint32_t audio_rate = 24000;
struct AudioMix {
    const int16_t *speech = nullptr;
    size_t samples = 0, position = 0;
    uint32_t tone_left = 0, tone_position = 0;
    uint16_t tone_hz = 0;

    bool active() const { return position < samples || tone_left; }
    void set_pcm(const int16_t *pcm, size_t count) {
        speech = pcm;
        samples = count;
        position = 0;
    }
    void set_tone(uint16_t hz, uint16_t ms) {
        tone_hz = hz;
        tone_position = 0;
        tone_left = hz && hz < audio_rate / 2 ? uint32_t(ms) * audio_rate / 1000 : 0;
    }
    void render(int16_t *out, size_t count, uint8_t volume) {
        for (size_t i = 0; i < count; ++i) {
            int32_t sample = position < samples ? speech[position++] : 0;
            if (tone_left) {
                sample += int32_t(std::sin(6.283185307179586 * tone_hz * tone_position++ / audio_rate) * 7000);
                --tone_left;
            }
            // Mix before clipping, then apply the user's volume once.
            out[i] = std::clamp<int32_t>(sample, -32768, 32767) * int(volume) / 255;
        }
    }
};
inline int16_t resample(const int16_t *pcm, size_t frames, bool stereo, uint32_t input_rate, size_t output_index) {
    const uint64_t pos = uint64_t(output_index) * input_rate;
    const size_t a = std::min<size_t>(pos / audio_rate, frames - 1), b = std::min(a + 1, frames - 1);
    const int32_t frac = pos % audio_rate;
    auto sample = [&](size_t i) -> int32_t { return stereo ? (int32_t(pcm[i * 2]) + pcm[i * 2 + 1]) / 2 : pcm[i]; };
    return (sample(a) * (int32_t(audio_rate) - frac) + sample(b) * frac) / int32_t(audio_rate);
}
} // namespace ws
