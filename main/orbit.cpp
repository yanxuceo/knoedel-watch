#include "orbit.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <nvs.h>

#include "audio_player.h"
#include "board.h"
#include "card_ui.h"
#include "deck.h"
#include "motion.h"
#include "orbit_game.h"
#include "orbit_sound.h"
#include "ui_strings.h"

namespace orbit {

static const char *TAG = "orbit";

// The lean that counts as full tilt, in g (about 25 degrees). The sensor's axes
// lie a quarter turn from the screen's: held keys up, leaning right lowers its
// y, leaning toward you its x (tried on the StopWatch).
static constexpr float FULL_TILT = 0.42f;

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

// ---- the motor: one pattern at a time, played out frame by frame ----------------------

static const uint16_t *buzz_pattern;
static int buzz_step;
static uint8_t buzz_level;
static uint32_t buzz_until;

static void buzz(const uint16_t *pattern, uint8_t level)
{
    buzz_pattern = pattern;
    buzz_step = 0;
    buzz_level = level;
    buzz_until = 0;
}

static void buzz_tick()
{
    if (buzz_pattern == nullptr || millis() < buzz_until) {
        return;
    }
    const uint16_t ms = buzz_pattern[buzz_step];
    board::vibrate(ms && buzz_step % 2 == 0 ? buzz_level : 0);
    if (ms == 0) {
        buzz_pattern = nullptr;
        return;
    }
    buzz_step++;
    buzz_until = millis() + ms;
}

// ---- sound: effects on channels of their own, so that they do not cut each other off ----

static constexpr uint8_t CH_BIG = 1, CH_MUSIC = 2, CH_SHOTS = 3, CH_HITS = 4;

static void sound(orbit_sound::Id id)
{
    const orbit_sound::Clip c = orbit_sound::clip(id);
    if (c.pcm == nullptr) {
        return;
    }
    using namespace orbit_sound;
    const uint8_t ch = id == SHOOT || id == TICK                                   ? CH_SHOTS
                       : id == KILL || id == GRAZE || id == COMBO || id == CHARGED ? CH_HITS
                                                                                   : CH_BIG;
    board::play_sound(c.pcm, c.samples, RATE, ch, false);
}

// The music the game wants, and what plays: they differ until the loops are made.
static int music_wanted, music_playing;

static void music(int level) { music_wanted = level; }

static void music_tick()
{
    if (music_wanted == music_playing) {
        return;
    }
    static const int16_t silence[32] = {};
    const orbit_sound::Clip c = orbit_sound::clip(music_wanted == 2 ? orbit_sound::MUSIC_HOT : orbit_sound::MUSIC_CALM);
    if (music_wanted == 0) {
        board::play_sound(silence, 32, orbit_sound::RATE, CH_MUSIC, false);  // replaces the loop: the music stops
    } else if (c.pcm != nullptr) {
        board::play_sound(c.pcm, c.samples, orbit_sound::RATE, CH_MUSIC, true);
    } else {
        return;  // not made yet
    }
    music_playing = music_wanted;
}

// The sounds are made once, in the background, the first time the game opens
// (about a second); the game starts meanwhile, and is silent until they are there.
static void make_task(void *)
{
    if (!orbit_sound::make([] { vTaskDelay(1); })) {
        ESP_LOGW(TAG, "no memory for the sounds: playing without");
    }
    vTaskDelete(nullptr);
}

static void make_sounds()
{
    static bool started;
    if (!started) {
        started = true;
        const uint32_t t0 = millis();
        if (xTaskCreatePinnedToCore(make_task, "orbit_sound", 4096, nullptr, 1, nullptr, 1) != pdPASS) {
            orbit_sound::make();
        }
        ESP_LOGI(TAG, "making the sounds (started in %lu ms)", (unsigned long)(millis() - t0));
    }
}

// ---- saved: the best score and the steering ------------------------------------------

static uint32_t load(const char *key, uint32_t fallback)
{
    nvs_handle_t h;
    uint32_t v = fallback;
    if (nvs_open("orbit", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, key, &v);
        nvs_close(h);
    }
    return v;
}

static void save(const char *key, uint32_t v)
{
    nvs_handle_t h;
    if (nvs_open("orbit", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, key, v);
        nvs_commit(h);
        nvs_close(h);
    }
}

// ---- showing frames: pushed by a task on the other core while the next is drawn ------
// A whole frame takes about 32 ms to reach the screen, as long as drawing one:
// one after the other the game ran at 16 fps.

struct Job {
    lgfx::LGFX_Sprite *frame;
    int x, y;
};
static QueueHandle_t jobs;
static SemaphoreHandle_t pushed;

// Only what the round screen shows: in bands, each as wide as the circle there
// (a fifth less to send than the square).
static void push_round(lgfx::LGFX_Sprite *frame, int x, int y)
{
    lgfx::LGFX_Device &d = board::display();
    constexpr int S = ui::SCREEN, BAND = 39;
    constexpr float R = S / 2.0f;
    d.startWrite();
    for (int y0 = 0; y0 < S; y0 += BAND) {
        const int h = y0 + BAND > S ? S - y0 : BAND;
        const float dy = y0 + h <= R ? R - (y0 + h) : y0 >= R ? y0 - R : 0;  // the band's row nearest the middle
        const int half = (int)ceilf(sqrtf(fmaxf(0, R * R - dy * dy))) + 1;
        d.setClipRect(x + (int)R - half, y + y0, 2 * half, h);
        frame->pushSprite(&d, x, y);
    }
    d.clearClipRect();
    d.endWrite();
}

static void push_task(void *)
{
    Job j;
    while (true) {
        if (xQueueReceive(jobs, &j, portMAX_DELAY) == pdTRUE) {
            push_round(j.frame, j.x, j.y);
            xSemaphoreGive(pushed);
            vTaskDelay(1);  // the core's idle task must get a turn now and then
        }
    }
}

static bool start_pusher()
{
    if (jobs != nullptr) {
        return true;
    }
    jobs = xQueueCreate(1, sizeof(Job));
    pushed = xSemaphoreCreateBinary();
    if (jobs == nullptr || pushed == nullptr ||
        xTaskCreatePinnedToCore(push_task, "orbit_push", 4096, nullptr, 2, nullptr, 1) != pdPASS) {
        ESP_LOGE(TAG, "no push task: frames are pushed in turn");
        return false;
    }
    return true;
}

// ---- the game loop -------------------------------------------------------------------

void run(lgfx::LGFX_Sprite *fb, int ox, int oy)
{
    audio_stop();
    board::stop_sound();
    board::wait_keys_released();  // the tap that opened the game does not also start it
    make_sounds();
    music_wanted = music_playing = 0;

    // A second frame, so that one is drawn while the other goes to the screen.
    lgfx::LGFX_Sprite back(&board::display());
    back.setPsram(true);
    back.setColorDepth(16);
    const bool two = start_pusher() && back.createSprite(ui::SCREEN, ui::SCREEN) != nullptr;
    lgfx::LGFX_Sprite *frames[2] = {fb, two ? &back : fb};
    int cur = 0;
    if (two) {
        xSemaphoreGive(pushed);  // nothing on its way yet
    }

    uint32_t saved_best = load("best", 0);
    bool marble = load("marble", 1) != 0;
    begin({sound, music, buzz}, saved_best, esp_random());
    set_clock([]() -> uint64_t { return (uint64_t)esp_timer_get_time(); });
    set_marble(marble);

    float gx = 0, gy = 0, base_x = 0, base_y = 0;  // the lean, smoothed, and where it counts as level
    bool have_lean = false;
    State last = state();
    uint32_t prev = millis(), log_ms = prev, draw_ms = 0, wait_ms = 0;
    int count = 0;

    while (true) {
        Input in;
        const board::Event ev = board::update();
        if (ev == board::Event::BACK) {
            break;
        }
        in.jump = ev == board::Event::PREVIOUS;
        if (ev == board::Event::SELECT && state() == TITLE) {
            marble = !marble;
            set_marble(marble);
            save("marble", marble);
            sound(orbit_sound::TICK);
        }
        in.fire = board::key_down(true);

        motion::update();
        in.bomb = motion::shaken();
        if (motion::face_down() && state() == PLAY && !paused()) {
            set_paused(true);
        }
        float ax, ay, az;
        if (board::acceleration(ax, ay, az)) {
            if (!have_lean) {
                gx = base_x = ax, gy = base_y = ay;
                have_lean = true;
            }
            gx += (ax - gx) * 0.4f;
            gy += (ay - gy) * 0.4f;
        }
        // The way the watch is held when a game starts counts as level.
        if (state() == PLAY && last != PLAY) {
            base_x = gx, base_y = gy;
        }
        if (state() == OVER && best() > saved_best) {
            saved_best = best();
            save("best", saved_best);
        }
        last = state();
        in.tilt_x = -(gy - base_y) / FULL_TILT;
        in.tilt_y = -(gx - base_x) / FULL_TILT;

        const uint32_t now = millis();
        update((now - prev) / 1000.0f > 0.05f ? 0.05f : (now - prev) / 1000.0f, in);
        prev = now;
        buzz_tick();
        music_tick();

        draw(frames[cur], now / 1000.0f, ui_text(deck_locale()));
        const uint32_t drawn = millis();
        if (two) {
            xSemaphoreTake(pushed, portMAX_DELAY);  // the frame before has reached the screen
            const Job j = {frames[cur], ox, oy};
            xQueueSend(jobs, &j, portMAX_DELAY);
            cur ^= 1;
        } else {
            push_round(fb, ox, oy);
        }
        draw_ms += drawn - now;
        wait_ms += millis() - drawn;
        count++;
        if (now - log_ms >= 3000) {
            int e, p, pa;
            counts(e, p, pa);
            uint32_t us[PROFILE_PARTS];
            profile(us);
            ESP_LOGI(TAG, "%.1f fps (draw %lu ms: back %lu, things %lu, ship %lu, hud %lu; wait %lu ms) enemies %d pellets %d parts %d | accel %.2f %.2f -> tilt %.2f %.2f",
                     count * 1000.0f / (now - log_ms), (unsigned long)(draw_ms / count), (unsigned long)(us[0] / count / 1000),
                     (unsigned long)(us[1] / count / 1000), (unsigned long)(us[2] / count / 1000),
                     (unsigned long)(us[3] / count / 1000), (unsigned long)(wait_ms / count), e, p, pa, gx, gy,
                     in.tilt_x, in.tilt_y);
            count = 0, draw_ms = wait_ms = 0, log_ms = now;
        }
        vTaskDelay(1);  // let the other tasks of this core run
    }

    if (two) {
        xSemaphoreTake(pushed, portMAX_DELAY);  // the last frame is out: `back` can go
    }
    back.deleteSprite();
    board::vibrate(0);
    board::stop_sound();
    board::wait_keys_released();
}

}  // namespace orbit
