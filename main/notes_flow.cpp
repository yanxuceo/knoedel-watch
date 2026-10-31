#include "notes_flow.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include <string>

#include "audio_player.h"
#include "board.h"
#include "credentials.h"
#include "memos.h"
#include "notes_sync.h"

namespace notes_flow {

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

// A dot, a title, a line and, with `level` >= 0, a level bar.
static void screen(const char *title, const char *line, uint32_t colour, float level = -1)
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
        d.fillRect(cx - 150, cy + 65, (int)(300 * (level > 1 ? 1 : level)), 10, colour);
    }
    d.endWrite();
}

static void progress(int index, int count)
{
    char line[32];
    snprintf(line, sizeof line, "memo %d of %d", index + 1, count);
    screen("Online", line, 0x5B93FFu);
}

void talk()
{
    audio_stop();
    board::stop_sound();
    board::display().fillScreen(TFT_BLACK);
    if (!memos::record_start()) {
        screen("Mic?", "the recording did not start", 0x8A867Eu);
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    uint32_t drawn = 0;
    while (board::talk_key_held() && memos::record_poll()) {
        if (millis() - drawn >= 150) {
            drawn = millis();
            char line[32];
            snprintf(line, sizeof line, "%.0f s  -  let go to keep", memos::record_seconds());
            screen("REC", line, 0xFF5D52u, memos::level() * 2);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    const float seconds = memos::record_seconds();
    const std::string id = memos::record_stop(seconds >= 1.0f);
    if (id.empty()) {
        screen("Too short", "hold the key while you talk", 0x8A867Eu);
        vTaskDelay(pdMS_TO_TICKS(1200));
        return;
    }
    if (!credentials::has_key()) {
        screen("Kept", "no OpenAI key on the watch yet", 0xFFB547u);
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    screen("Kept", "going online", 0x4FD1BFu);
    const notes_sync::Result r = notes_sync::run(progress);
    char line[48];
    if (r.limited) {
        snprintf(line, sizeof line, "monthly limit reached, %d waiting", r.left);
        screen("Kept", line, 0xFFB547u);
    } else if (r.offline) {
        snprintf(line, sizeof line, "offline, %d waiting", r.left);
        screen("Kept", line, 0xFFB547u);
    } else {
        snprintf(line, sizeof line, r.left ? "%d notes, %d still waiting" : "%d new notes", r.done, r.left);
        screen(r.done ? "Done" : "Failed", line, r.done ? 0x4FD1BFu : 0xFF6B5Eu);
    }
    vTaskDelay(pdMS_TO_TICKS(1800));
}

}  // namespace notes_flow
