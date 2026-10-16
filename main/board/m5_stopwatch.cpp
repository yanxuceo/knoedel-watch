// M5Stack StopWatch Dev Kit (C152): 468x468 CO5300 AMOLED with touch, KEY A
// (yellow, left) and KEY B (blue, right), speaker, BMI270, RX8130 clock, M5PM1.
// M5Unified knows all of it.
//
//   KEY A : previous     hold KEY A : back     swipe right : previous
//   KEY B : next         tap        : select   swipe left  : next

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
    auto cfg = M5.config();
    cfg.internal_mic = false;
    M5.begin(cfg);

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
    if (M5.BtnB.wasClicked()) {
        return Event::NEXT;
    }
    if (touch.wasFlicked() && abs(touch.distanceX()) > abs(touch.distanceY())) {
        return touch.distanceX() < 0 ? Event::NEXT : Event::PREVIOUS;
    }
    if (touch.wasClicked()) {
        return Event::SELECT;
    }
    return Event::NONE;
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
