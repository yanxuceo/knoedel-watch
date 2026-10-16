#include "calendar.h"
#include <esp_log.h>
#include <nvs.h>
#include <sys/time.h>
#include <time.h>

namespace {
bool clock_valid = false;
// A new namespace avoids treating the old port's timezone-specific epoch as
// timezone-free civil time. A first upgrade requires setting the clock again.
constexpr const char *clock_namespace = "ws_clock_v2";
} // namespace
namespace ws {
void save_clock() {
    if (!clock_valid || !valid_epoch(time(nullptr)))
        return;
    nvs_handle_t handle;
    if (nvs_open(clock_namespace, NVS_READWRITE, &handle) != ESP_OK)
        return;
    esp_err_t err = nvs_set_i64(handle, "civil_epoch", time(nullptr));
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK)
        ESP_LOGW("ws-clock", "Could not save clock: %s", esp_err_to_name(err));
}
void clock_begin() {
    int64_t epoch = 0;
    nvs_handle_t handle;
    if (nvs_open(clock_namespace, NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_i64(handle, "civil_epoch", &epoch);
        nvs_close(handle);
    }
    if (valid_epoch(epoch)) {
        timeval tv = {.tv_sec = time_t(epoch), .tv_usec = 0};
        clock_valid = settimeofday(&tv, nullptr) == 0;
    }
    ESP_LOGI("ws-clock", "%s", clock_valid ? "restored last saved local time" : "clock has not been set");
}
} // namespace ws
namespace board {
bool has_clock() { return true; }
bool get_time(DateTime &out) {
    time_t epoch = time(nullptr);
    if (!clock_valid || !ws::valid_epoch(epoch))
        return false;
    tm t = {};
    if (!gmtime_r(&epoch, &t))
        return false;
    out = {int16_t(t.tm_year + 1900), int8_t(t.tm_mon + 1), int8_t(t.tm_mday),
           int8_t(t.tm_hour),         int8_t(t.tm_min),     int8_t(t.tm_sec)};
    return true;
}
bool set_time(const DateTime &t) {
    if (!ws::valid_date(t))
        return false;
    timeval tv = {.tv_sec = time_t(ws::civil_epoch(t)), .tv_usec = 0};
    if (settimeofday(&tv, nullptr) != 0)
        return false;
    clock_valid = true;
    ws::save_clock();
    return true;
}
} // namespace board
