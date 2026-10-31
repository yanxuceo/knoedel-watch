#include "notes_sync.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <atomic>

#include <nvs.h>

#include <vector>

#include "board.h"
#include "credentials.h"
#include "memos.h"
#include "net.h"
#include "openai.h"

namespace notes_sync {

static const char *TAG = "notes_sync";

// The day a memo was said, from its id "20261031-111508" (noon, so that
// adding days never meets a change of summer time); false when unknown.
static bool said_on(const std::string &id, struct tm &day)
{
    day = {};
    if (sscanf(id.c_str(), "%4d%2d%2d", &day.tm_year, &day.tm_mon, &day.tm_mday) != 3 || day.tm_year < 2001) {
        return false;
    }
    day.tm_year -= 1900;
    day.tm_mon -= 1;
    day.tm_hour = 12;
    mktime(&day);  // sets the weekday
    return true;
}

// "Today: Saturday, 2026-10-31": what "明天" or "十一月十号" are counted from.
static std::string context(const std::string &id)
{
    static const char *DAY[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    struct tm t;
    if (!said_on(id, t)) {
        return "Today: unknown";
    }
    char buf[48];
    snprintf(buf, sizeof buf, "Today: %s, %04d-%02d-%02d", DAY[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return buf;
}

// A task's day as the model heard it (openai.h), counted from the day the memo
// was said: "下周三" is the Wednesday of the week after, "周五" the next Friday,
// "明天" the day after. The ISO date, or "" for none.
static std::string count_day(const cJSON *when, const std::string &id)
{
    struct tm day;
    const cJSON *type = cJSON_GetObjectItem(when, "type");
    if (!cJSON_IsString(type) || !said_on(id, day)) {
        return "";
    }
    const cJSON *days = cJSON_GetObjectItem(when, "days"), *weekday = cJSON_GetObjectItem(when, "weekday"),
                *date = cJSON_GetObjectItem(when, "date");
    int add = -1;
    if (strcmp(type->valuestring, "days") == 0 && cJSON_IsNumber(days) && days->valueint >= 0 && days->valueint < 400) {
        add = days->valueint;
    } else if (strcmp(type->valuestring, "weekday") == 0 && cJSON_IsNumber(weekday) && weekday->valueint >= 1 &&
               weekday->valueint <= 7) {
        const int today = day.tm_wday == 0 ? 7 : day.tm_wday, want = weekday->valueint;  // Monday 1 .. Sunday 7
        if (cJSON_IsTrue(cJSON_GetObjectItem(when, "next_week"))) {
            add = 7 - (today - 1) + (want - 1);  // to Monday of next week, then to the day
        } else {
            add = (want - today + 7) % 7;
            add = add ? add : 7;  // "周五" said on a Friday: the next one
        }
    } else if (strcmp(type->valuestring, "date") == 0 && cJSON_IsString(date)) {
        int y, m, d;
        if (sscanf(date->valuestring, "%4d-%2d-%2d", &y, &m, &d) == 3 && y > 2000 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
            char buf[40];
            snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
            return buf;
        }
    }
    if (add < 0) {
        return "";
    }
    day.tm_mday += add;
    mktime(&day);
    char buf[40];
    snprintf(buf, sizeof buf, "%04d-%02d-%02d", day.tm_year + 1900, day.tm_mon + 1, day.tm_mday);
    return buf;
}

// The tidied note with each task's "when" turned into "due".
static void count_days(cJSON *note, const std::string &id)
{
    cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(note, "items"))
    {
        const std::string due = count_day(cJSON_GetObjectItem(item, "when"), id);
        cJSON_DeleteItemFromObject(item, "when");
        if (due.empty()) {
            cJSON_AddNullToObject(item, "due");
        } else {
            cJSON_AddStringToObject(item, "due", due.c_str());
        }
    }
}

// One go: online, every memo waiting, offline again.
static Result go()
{
    Result r;
    const auto ids = memos::waiting();
    r.left = (int)ids.size();
    if (ids.empty() || !credentials::has_key()) {
        return r;
    }
    const int64_t t0 = esp_timer_get_time();
    if (!net::connect()) {
        r.offline = true;
        return r;
    }
    // The clock once a day is enough.
    static int64_t synced_us = -1;
    if (synced_us < 0 || esp_timer_get_time() - synced_us > 24LL * 3600 * 1000000) {
        if (net::sync_time()) {
            synced_us = esp_timer_get_time();
        }
    }
    const int64_t online_us = esp_timer_get_time();
    openai::begin_session();

    for (const std::string &id : ids) {
        size_t size = 0;
        uint8_t *ogg = memos::read(id, size);
        if (ogg == nullptr) {
            continue;
        }
        const int64_t a = esp_timer_get_time();
        const std::string text = openai::transcribe_ogg(ogg, size);
        free(ogg);
        const int64_t b = esp_timer_get_time();
        if (text.empty() && openai::status() != 200) {
            r.limited = openai::spend_limited();
            break;  // try again next time
        }
        // A tidying that fails leaves the transcript as an idea: nothing said is lost.
        const std::string note = text.empty() ? "" : openai::tidy(text, context(id).c_str());
        const int64_t c = esp_timer_get_time();
        cJSON *n = note.empty() ? nullptr : cJSON_Parse(note.c_str());
        if (n != nullptr) {
            count_days(n, id);
        }
        if (n == nullptr) {
            n = cJSON_CreateObject();
            cJSON_AddStringToObject(n, "kind", "idea");
            cJSON_AddArrayToObject(n, "items");
            cJSON_AddStringToObject(n, "text", text.c_str());
        }
        cJSON *saved = cJSON_CreateObject();
        cJSON_AddStringToObject(saved, "id", id.c_str());
        cJSON_AddStringToObject(saved, "transcript", text.c_str());
        cJSON_AddItemToObject(saved, "note", n);
        char *json = cJSON_PrintUnformatted(saved);
        cJSON_Delete(saved);
        const bool ok = memos::finish(id, json);
        ESP_LOGI(TAG, "%s: %u bytes, transcript in %lld ms, note in %lld ms%s: %s", id.c_str(), (unsigned)size,
                 (b - a) / 1000, (c - b) / 1000, ok ? "" : " (NOT WRITTEN)", json);
        if (ok) {
            r.done++;
            r.last = json;
        }
        cJSON_free(json);
        if (!ok) {
            break;
        }
    }
    openai::end_session();
    net::disconnect();
    r.left = (int)memos::waiting().size();
    ESP_LOGI(TAG, "%d done, %d left; online in %lld ms, all in %lld ms", r.done, r.left, (online_us - t0) / 1000,
             (esp_timer_get_time() - t0) / 1000);
    return r;
}

// ---- when to go -------------------------------------------------------------------

static constexpr int BATCH = 3, LOW_BATTERY = 20;
static constexpr int64_t OLDEST_WAITS_S = 15 * 60;

static TaskHandle_t handle;
static SemaphoreHandle_t lock;  // guards `latest`
static Result latest;
static std::atomic<uint32_t> count;
static std::atomic<bool> running, page, saving_on{true};

// Seconds since 2000 of a memo's id, or of the clock now; -1 when unknown.
static int64_t seconds(int y, int mo, int d, int h, int mi, int s)
{
    struct tm t = {};
    t.tm_year = y - 1900, t.tm_mon = mo - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = mi, t.tm_sec = s;
    return (int64_t)mktime(&t) - 946684800;
}

static int64_t waited_s(const std::string &id)
{
    int y, mo, d, h, mi, s;
    board::DateTime now = {};
    if (sscanf(id.c_str(), "%4d%2d%2d-%2d%2d%2d", &y, &mo, &d, &h, &mi, &s) != 6 || y < 2001 || !board::get_time(now)) {
        return OLDEST_WAITS_S;  // unknown: as if it had waited long enough
    }
    return seconds(now.year, now.month, now.day, now.hour, now.minute, now.second) - seconds(y, mo, d, h, mi, s);
}

static bool time_to_go(const std::vector<std::string> &ids)
{
    if (board::on_usb() || page || !saving_on) {
        return true;
    }
    const int battery = board::battery_level();
    if (battery >= 0 && battery < LOW_BATTERY) {
        return false;
    }
    return (int)ids.size() >= BATCH || waited_s(ids.front()) >= OLDEST_WAITS_S;
}

// ---- the task ---------------------------------------------------------------------

static void task(void *)
{
    static const uint32_t RETRY_MIN[] = {1, 5, 15, 60};
    int retry = 0;
    int64_t not_before_us = 0;  // after a failure
    while (true) {
        const auto ids = memos::waiting();
        // Asleep without memos; with some, a look every minute (cheap: no Wi-Fi).
        const bool asked = ulTaskNotifyTake(pdTRUE, ids.empty() ? portMAX_DELAY : pdMS_TO_TICKS(60 * 1000)) > 0;
        const auto now_ids = memos::waiting();
        if (now_ids.empty() || !credentials::has_key()) {
            continue;
        }
        if (asked) {
            not_before_us = 0;  // asked again: a new memo, the page opened
        }
        if (esp_timer_get_time() < not_before_us || !time_to_go(now_ids)) {
            continue;
        }
        running = true;
        const Result r = go();
        running = false;
        xSemaphoreTake(lock, portMAX_DELAY);
        latest = r;
        xSemaphoreGive(lock);
        count++;
        if (r.left == 0 || r.done > 0) {
            retry = 0;
            not_before_us = 0;
        } else {
            not_before_us = esp_timer_get_time() + (int64_t)RETRY_MIN[retry] * 60 * 1000000;
            ESP_LOGI(TAG, "%d memos waiting: next try in %lu min", r.left, (unsigned long)RETRY_MIN[retry]);
            retry = retry < 3 ? retry + 1 : 3;
        }
    }
}

// TLS and the JSON of the answers want more than the default stack; internal
// RAM, as Wi-Fi and flash are used from it. On core 1 and below the main
// task's priority: TLS's handshake is seconds of arithmetic, which on core 0
// made the screen and the keys slow.
void start()
{
    nvs_handle_t h;
    uint8_t v = 1;
    if (nvs_open("notes", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "saving", &v);
        nvs_close(h);
    }
    saving_on = v != 0;
    lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(task, "notes_sync", 16 * 1024, nullptr, 1, &handle, 1);
    request();  // memos from before: looked at now
}

void request()
{
    if (handle != nullptr) {
        xTaskNotifyGive(handle);
    }
}

bool busy() { return running; }

bool goes_now()
{
    const auto ids = memos::waiting();
    return !ids.empty() && credentials::has_key() && time_to_go(ids);
}

void page_open(bool open)
{
    page = open;
    if (open) {
        request();
    }
}

bool saving() { return saving_on; }

void set_saving(bool on)
{
    saving_on = on;
    nvs_handle_t h;
    if (nvs_open("notes", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "saving", on ? 1 : 0);
        nvs_commit(h);
        nvs_close(h);
    }
    request();
}

Result last(uint32_t *n)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    Result r = latest;
    xSemaphoreGive(lock);
    if (n != nullptr) {
        *n = count;
    }
    return r;
}

}  // namespace notes_sync
