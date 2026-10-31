#include "notes_sync.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <atomic>

#include "credentials.h"
#include "memos.h"
#include "net.h"
#include "openai.h"

namespace notes_sync {

static const char *TAG = "notes_sync";

// What the model needs to turn "明天" or "Freitag" into a date: the day the
// memo was said (its id, "20261031-111508") and the two weeks after it. Left
// to count days itself, the model once put a Friday three weeks late.
static std::string context(const std::string &id)
{
    struct tm t = {};
    if (sscanf(id.c_str(), "%4d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday) != 3 || t.tm_year < 2001) {
        return "Today: unknown";
    }
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_hour = 12;
    static const char *DAY[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    std::string s;
    for (int d = 0; d < 15; d++) {
        struct tm day = t;
        day.tm_mday += d;
        mktime(&day);  // normalises the date and sets the weekday
        char buf[48];
        if (d == 0) {
            snprintf(buf, sizeof buf, "Today: %s, %04d-%02d-%02d\nCalendar: ", DAY[day.tm_wday], day.tm_year + 1900,
                     day.tm_mon + 1, day.tm_mday);
            s += buf;
        }
        snprintf(buf, sizeof buf, "%s%.3s %04d-%02d-%02d", d ? ", " : "", DAY[day.tm_wday], day.tm_year + 1900,
                 day.tm_mon + 1, day.tm_mday);
        s += buf;
    }
    return s;
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
    net::disconnect();
    r.left = (int)memos::waiting().size();
    ESP_LOGI(TAG, "%d done, %d left; online in %lld ms, all in %lld ms", r.done, r.left, (online_us - t0) / 1000,
             (esp_timer_get_time() - t0) / 1000);
    return r;
}

// ---- the task ---------------------------------------------------------------------

static TaskHandle_t handle;
static SemaphoreHandle_t lock;  // guards `latest`
static Result latest;
static std::atomic<uint32_t> count;
static std::atomic<bool> running;

static void task(void *)
{
    static const uint32_t RETRY_MIN[] = {1, 5, 15, 60};
    int retry = 0;
    TickType_t wait = 0;  // memos from before go at once
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, wait) > 0) {
            retry = 0;  // asked: a new memo
        }
        if (memos::waiting().empty() || !credentials::has_key()) {
            wait = portMAX_DELAY;
            continue;
        }
        running = true;
        const Result r = go();
        running = false;
        xSemaphoreTake(lock, portMAX_DELAY);
        latest = r;
        xSemaphoreGive(lock);
        count++;
        if (r.left == 0) {
            retry = 0;
            wait = portMAX_DELAY;
        } else {
            wait = pdMS_TO_TICKS(RETRY_MIN[retry] * 60 * 1000);
            ESP_LOGI(TAG, "%d memos waiting: next try in %lu min", r.left, (unsigned long)RETRY_MIN[retry]);
            retry = retry < 3 ? retry + 1 : 3;
        }
    }
}

// TLS and the JSON of the answers want more than the default stack; internal
// RAM, as Wi-Fi and flash are used from it.
void start()
{
    lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(task, "notes_sync", 16 * 1024, nullptr, 3, &handle, 0);
}

void request()
{
    if (handle != nullptr) {
        xTaskNotifyGive(handle);
    }
}

bool busy() { return running; }

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
