#include "notes_flow.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>

#include "audio_player.h"
#include "board.h"
#include "credentials.h"
#include "memos.h"
#include "net.h"
#include "notes_sync.h"
#include "notes_ui.h"

namespace notes_flow {

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

// A message after letting go, played as a short animation.
static void message(lgfx::LovyanGFX *fb, void (*push)(), notes_ui::Message m, uint32_t ms)
{
    const uint32_t t0 = millis();
    for (uint32_t now = t0; now - t0 < ms; now = millis()) {
        notes_ui::draw_message(fb, m, (now - t0) / (float)ms * (ms / 700.0f));
        push();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

void talk(lgfx::LovyanGFX *fb, void (*push)())
{
    audio_stop();
    board::stop_sound();
    if (!memos::record_start()) {
        message(fb, push, notes_ui::CANCELLED, 900);
        return;
    }
    bool cancelled = false;
    while (memos::record_poll()) {
        if (!board::talk_key_held()) {
            break;
        }
        if (board::cancel_key_held()) {
            cancelled = true;
            break;
        }
        notes_ui::draw_recording(fb, memos::record_seconds(), memos::MAX_SECONDS, memos::level(), millis() / 1000.0f);
        push();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const float seconds = memos::record_seconds();
    const std::string id = memos::record_stop(!cancelled && seconds >= 1.0f);
    board::wait_keys_released();  // the keys used here do not also click
    if (cancelled) {
        message(fb, push, notes_ui::CANCELLED, 900);
        return;
    }
    if (id.empty()) {
        message(fb, push, notes_ui::TOO_SHORT, 1000);
        return;
    }
    // The note is made in the background (notes_sync.h): back to where the watch was.
    notes_sync::request();
    const notes_ui::Message m = !credentials::has_key() ? notes_ui::KEPT_NO_KEY
                                : notes_sync::last().offline ? notes_ui::KEPT_OFFLINE
                                                             : notes_ui::KEPT_ONLINE;
    message(fb, push, m, m == notes_ui::KEPT_ONLINE ? 800 : 1300);
}

}  // namespace notes_flow
