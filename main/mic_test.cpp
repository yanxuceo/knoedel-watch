// Microphone test (CONFIG_KNOEDEL_MIC_TEST): while the right key is held, the
// watch records 16 kHz mono. With an OpenAI key on the watch (usb_commands.h)
// it then goes online itself, transcribes and tidies the memo and logs both.
// Then the recording goes out over USB as base64 lines between
// "MIC_BEGIN <rate> <samples>" and "MIC_END", which tools/mic_test.py turns
// into a WAV file. The log tells how loud it was, how long the codec stayed
// silent after starting and how long each step online took. Not for a release.

#include "mic_test.h"

#include <driver/usb_serial_jtag.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "audio_player.h"
#include "board.h"
#include "credentials.h"
#include "memos.h"
#include "net.h"
#include "openai.h"

namespace mic_test {

static const char *TAG = "mic_test";
static constexpr uint32_t RATE = 16000;
static constexpr size_t CHUNK = RATE / 10;  // 100 ms per request
static constexpr size_t MAX = RATE * 30;    // 30 s

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

// A dot, a title, a line and, with `level` >= 0, a level bar.
static void screen(const char *title, const char *line, uint32_t colour, float level)
{
    auto &d = board::display();
    const int cx = d.width() / 2, cy = d.height() / 2;
    d.startWrite();
    d.fillRect(cx - 200, cy - 110, 400, 220, TFT_BLACK);
    d.fillCircle(cx, cy - 80, 12, colour);
    d.setTextDatum(middle_center);
    d.setFont(&fonts::DejaVu40);
    d.setTextColor(0xF5F2EAu, TFT_BLACK);
    d.drawString(title, cx, cy - 25);
    d.setFont(&fonts::DejaVu18);
    d.setTextColor(0x9A968Eu, TFT_BLACK);
    d.drawString(line, cx, cy + 25);
    if (level >= 0) {
        d.fillRect(cx - 150, cy + 65, 300, 10, 0x262626u);
        d.fillRect(cx - 150, cy + 65, (int)(300 * fminf(1.0f, level)), 10, colour);
    }
    d.endWrite();
}

// Base64 in lines of 76 characters (57 bytes).
static void send(const int16_t *pcm, size_t samples)
{
    static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint8_t *p = (const uint8_t *)pcm;
    const size_t n = samples * sizeof(int16_t);
    printf("\nMIC_BEGIN %u %u\n", (unsigned)RATE, (unsigned)samples);
    char line[80];
    for (size_t i = 0; i < n; i += 57) {
        const size_t m = n - i < 57 ? n - i : 57;
        int k = 0;
        for (size_t j = 0; j < m; j += 3) {
            const uint32_t v = (uint32_t)p[i + j] << 16 | (j + 1 < m ? (uint32_t)p[i + j + 1] << 8 : 0) |
                               (j + 2 < m ? p[i + j + 2] : 0);
            line[k++] = B64[v >> 18 & 63];
            line[k++] = B64[v >> 12 & 63];
            line[k++] = j + 1 < m ? B64[v >> 6 & 63] : '=';
            line[k++] = j + 2 < m ? B64[v & 63] : '=';
        }
        line[k++] = '\n';
        fwrite(line, 1, k, stdout);
    }
    printf("MIC_END\n");
    fflush(stdout);
}

// "Saturday, 2026-10-31, 10:45": the date the model counts "tomorrow" from.
static void now_text(char *out, size_t size)
{
    static const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const int OFFSET[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    board::DateTime t = {};
    if (!board::get_time(t)) {
        snprintf(out, size, "unknown");
        return;
    }
    const int y = t.year - (t.month < 3);
    const int weekday = (y + y / 4 - y / 100 + y / 400 + OFFSET[t.month - 1] + t.day) % 7;
    snprintf(out, size, "Today: %s, %04d-%02d-%02d", DAYS[weekday], t.year, t.month, t.day);
}

// Online as the notes will be: Wi-Fi, the clock, the transcript, the note.
static void online(const int16_t *pcm, size_t n)
{
    screen("Online", "connecting", 0x5B93FFu, -1);
    const uint32_t t0 = millis();
    if (!net::connect()) {
        screen("Offline", "no saved network answered", 0x8A867Eu, -1);
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    net::sync_time();
    const uint32_t t1 = millis();
    screen("Online", "transcribing", 0x5B93FFu, -1);
    const std::string text = openai::transcribe(pcm, n, RATE);
    const uint32_t t2 = millis();
    std::string note;
    if (!text.empty()) {
        char now[48];
        now_text(now, sizeof now);
        screen("Online", "tidying", 0x5B93FFu, -1);
        note = openai::tidy(text, now);
    }
    const uint32_t t3 = millis();
    net::disconnect();
    ESP_LOGI(TAG, "online in %lu ms, transcript in %lu ms, note in %lu ms", (unsigned long)(t1 - t0),
             (unsigned long)(t2 - t1), (unsigned long)(t3 - t2));
    ESP_LOGI(TAG, "transcript: %s", text.c_str());
    ESP_LOGI(TAG, "note: %s", note.c_str());
    char line[48];
    snprintf(line, sizeof line, "%.1f s after letting go", (t3 - t0) / 1000.0f);
    screen(note.empty() ? "Failed" : "Done", openai::spend_limited() ? "monthly limit reached" : line,
           note.empty() ? 0xFF6B5Eu : 0x4FD1BFu, -1);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

void run()
{
    static int16_t *pcm = nullptr;
    if (pcm == nullptr) {
        pcm = (int16_t *)heap_caps_malloc(MAX * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    }
    if (!board::has_mic() || pcm == nullptr) {
        ESP_LOGE(TAG, "no microphone or no memory for the recording");
        return;
    }
    audio_stop();
    board::stop_sound();
    board::display().fillScreen(TFT_BLACK);
    const uint32_t asked = millis();
    if (!board::mic_begin()) {
        ESP_LOGE(TAG, "the microphone did not start");
        screen("Mic?", "the microphone did not start", 0x8A867Eu, -1);
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    const uint32_t started = millis();
    size_t queued = 0;
    uint32_t drawn = 0;
    while (queued + CHUNK <= MAX && (queued == 0 || board::talk_key_held())) {
        if (board::mic_record(pcm + queued, CHUNK, RATE)) {
            queued += CHUNK;
            continue;
        }
        if (millis() - drawn >= 200) {
            drawn = millis();
            // The level of the newest 100 ms that are filled.
            const size_t filled = queued - (size_t)board::mic_busy() * CHUNK;
            int peak = 0;
            for (size_t i = filled >= CHUNK ? filled - CHUNK : 0; i < filled; i++) {
                peak = abs(pcm[i]) > peak ? abs(pcm[i]) : peak;
            }
            char line[48];
            snprintf(line, sizeof line, "%.1f s  -  release to send", (millis() - started) / 1000.0f);
            screen("REC", line, 0xFF5D52u, peak / 8192.0f);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    board::mic_end();

    // The last 0.1 s holds the click of the key being let go.
    const size_t n = queued > RATE / 10 ? queued - RATE / 10 : 0;
    memos::apply_gain(pcm, n);  // the board delivers raw samples
    size_t silent = 0;  // the codec warming up: samples at the start that are (almost) zero
    while (silent < n && abs(pcm[silent]) <= 2) {
        silent++;
    }
    int peak = 0;
    double sum = 0;
    for (size_t i = silent; i < n; i++) {
        peak = abs(pcm[i]) > peak ? abs(pcm[i]) : peak;
        sum += (double)pcm[i] * pcm[i];
    }
    const double rms = n > silent ? sqrt(sum / (n - silent)) : 0;
    ESP_LOGI(TAG, "%.2f s recorded (microphone up in %lu ms), %.2f s silent at the start, peak %d (%.1f dBFS), rms %.1f dBFS",
             n / (float)RATE, (unsigned long)(started - asked), silent / (float)RATE, peak,
             20 * log10(fmax(peak, 1) / 32768.0), 20 * log10(fmax(rms, 1) / 32768.0));

    if (credentials::has_key()) {
        online(pcm, n);
    }
    char line[48];
    snprintf(line, sizeof line, "%.1f s recorded", n / (float)RATE);
    if (!usb_serial_jtag_is_connected()) {
        screen("No USB", "connect a computer to send", 0x8A867Eu, -1);
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    screen("Sending", line, 0xFFB547u, -1);
    send(pcm, n);
    screen("Sent", line, 0x4FD1BFu, -1);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

}  // namespace mic_test
