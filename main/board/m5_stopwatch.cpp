// M5Stack StopWatch Dev Kit (C152): 468x468 CO5300 AMOLED with touch, KEY A
// (yellow, left) and KEY B (blue, right), speaker, BMI270, RX8130 clock, M5PM1.
// M5Unified knows all of it.
//
//   KEY A : previous     hold KEY A : back     swipe right : previous
//   KEY B : next         hold KEY B : talk     swipe left  : next
//                        tap        : select

#include "board.h"

#include <M5Unified.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>

namespace board {

static const char *TAG = "board";

void begin()
{
    // The microphone is set up here and started only by mic_begin().
    M5.begin(M5.config());
    auto mic = M5.Mic.config();
    // Raw samples: speech near the watch is about -45 dBFS (rms). The gain is
    // applied by the app with a soft limiter (memos.cpp): this magnification
    // clips hard, and at 12 the peaks already reached full scale.
    mic.magnification = 1;
    M5.Mic.config(mic);

    float ax = 0, ay = 0, az = 0;
    M5.Imu.update();
    M5.Imu.getAccel(&ax, &ay, &az);
    ESP_LOGI(TAG, "M5Stack StopWatch; imu: %s (type %d), accel %.2f %.2f %.2f g", M5.Imu.isEnabled() ? "found" : "not found",
             (int)M5.Imu.getType(), ax, ay, az);
    ESP_LOGI(TAG, "power: battery %d %% (%d mV), %s, VBUS %d mV", (int)M5.Power.getBatteryLevel(),
             (int)M5.Power.getBatteryVoltage(), M5.Power.isCharging() == m5::Power_Class::is_charging ? "charging" : "not charging",
             (int)M5.Power.getVBUSVoltage());
}

lgfx::LGFX_Device &display() { return M5.Display; }

static int tap_x, tap_y;

Event update()
{
    M5.update();
    const auto touch = M5.Touch.getDetail();
    if (M5.BtnA.wasHold()) {
        return Event::BACK;
    }
    if (M5.BtnA.wasClicked()) {
        return Event::PREVIOUS;
    }
    if (M5.BtnB.wasHold()) {
        return Event::TALK;
    }
    if (M5.BtnB.wasClicked()) {
        return Event::NEXT;
    }
    if (touch.wasFlicked() && abs(touch.distanceX()) > abs(touch.distanceY())) {
        return touch.distanceX() < 0 ? Event::NEXT : Event::PREVIOUS;
    }
    if (touch.wasClicked()) {
        tap_x = touch.x;
        tap_y = touch.y;
        return Event::SELECT;
    }
    return Event::NONE;
}

void tap_point(int &x, int &y)
{
    x = tap_x;
    y = tap_y;
}

bool cancel_key_held() { return M5.BtnA.isPressed(); }

void wait_keys_released()
{
    do {
        vTaskDelay(pdMS_TO_TICKS(10));
        M5.update();
    } while (M5.BtnA.isPressed() || M5.BtnB.isPressed());
    vTaskDelay(pdMS_TO_TICKS(20));
    M5.update();
}

bool touched() { return M5.Touch.getDetail().isPressed() || M5.BtnA.isPressed() || M5.BtnB.isPressed(); }

// ---- sound --------------------------------------------------------------------
// Speech plays on channel 0, tones on channel 1.

void set_volume(uint8_t volume) { M5.Speaker.setVolume(volume); }

void play_pcm(const int16_t *pcm, size_t samples, uint32_t rate, bool stereo)
{
    M5.Speaker.playRaw(pcm, samples, rate, stereo, 1, 0, true);
}

void stop_sound()
{
    M5.Speaker.stop();
    vTaskDelay(pdMS_TO_TICKS(20));  // let the mixer drop the old buffer before it is reused
}

void tone(uint16_t hz, uint16_t ms) { M5.Speaker.tone(hz, ms, 1); }

// ---- microphone -----------------------------------------------------------------
// The ES8311 serves both: the speaker's and the microphone's I2S ports share
// its clock pins, so only one of them runs at a time.

bool has_mic() { return M5.Mic.isEnabled(); }

bool talk_key_held()
{
    M5.update();
    return M5.BtnB.isPressed();
}

bool mic_begin()
{
    M5.Speaker.end();
    return M5.Mic.begin();
}

bool mic_record(int16_t *pcm, size_t samples, uint32_t rate)
{
    return M5.Mic.isRecording() < 2 && M5.Mic.record(pcm, samples, rate);
}

int mic_busy() { return (int)M5.Mic.isRecording(); }

void mic_end()
{
    while (M5.Mic.isRecording()) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    M5.Mic.end();
    M5.Speaker.begin();
}

// After power-up the ES8311 delivers zeros for about a second (measured: 0.99 s
// at every first recording, none after); it stays ready after mic_end().
void mic_warm_up()
{
    static int16_t chunk[800];  // 50 ms at 16 kHz
    const TickType_t t0 = xTaskGetTickCount();
    if (!mic_begin()) {
        return;
    }
    bool ready = false;
    while (!ready && pdTICKS_TO_MS(xTaskGetTickCount() - t0) < 2000) {
        if (M5.Mic.record(chunk, 800, 16000)) {
            while (M5.Mic.isRecording()) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            for (int16_t s : chunk) {
                ready |= s > 2 || s < -2;
            }
        }
    }
    mic_end();
    ESP_LOGI(TAG, "microphone %s after %lu ms", ready ? "ready" : "still silent", (unsigned long)pdTICKS_TO_MS(xTaskGetTickCount() - t0));
}

// ---- motion sensor --------------------------------------------------------------

bool has_motion() { return M5.Imu.isEnabled(); }

bool acceleration(float &x, float &y, float &z) { return M5.Imu.isEnabled() && M5.Imu.getAccel(&x, &y, &z); }

// ---- clock --------------------------------------------------------------------

bool has_clock() { return M5.Rtc.isEnabled(); }

// A read fails on a single bad byte from the shared I2C bus, so it is tried a
// few times: the app takes a failure to mean that the clock lost its time.
bool get_time(DateTime &t)
{
    if (!M5.Rtc.isEnabled()) {
        return false;
    }
    m5::rtc_datetime_t dt;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (M5.Rtc.getDateTime(&dt)) {
            if (attempt > 1) {
                ESP_LOGW(TAG, "clock read on attempt %d", attempt);
            }
            t = {dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds};
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // The RX8130's time registers (0x10..0x16) and flags (0x1D), to tell a
    // stopped clock from a bus that keeps failing.
    uint8_t regs[7] = {}, flags = 0;
    const bool read = M5.In_I2C.readRegister(0x32, 0x10, regs, sizeof(regs), 400000) &&
                      M5.In_I2C.readRegister(0x32, 0x1D, &flags, 1, 400000);
    ESP_LOGW(TAG, "clock holds no valid time: %s %02x %02x %02x %02x %02x %02x %02x, flags %02x",
             read ? "registers" : "unreadable", regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], flags);
    return false;
}

bool set_time(const DateTime &t)
{
    // The RX8130 keeps a weekday too: 0 Sunday .. 6 Saturday.
    static const int OFFSET[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    const int y = t.year - (t.month < 3);
    const int8_t weekday = (int8_t)((y + y / 4 - y / 100 + y / 400 + OFFSET[t.month - 1] + t.day) % 7);
    const m5::rtc_date_t date = {t.year, t.month, t.day, weekday};
    const m5::rtc_time_t time = {t.hour, t.minute, t.second};
    if (!M5.Rtc.isEnabled()) {
        return false;
    }
    M5.Rtc.setDateTime(&date, &time);
    return true;
}

// ---- power --------------------------------------------------------------------

bool on_usb() { return M5.Power.getVBUSVoltage() >= 4000; }

void power_off() { M5.Power.powerOff(); }

}  // namespace board
