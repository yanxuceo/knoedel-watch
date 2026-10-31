#include "notes_sync.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <stdio.h>
#include <stdlib.h>

#include "memos.h"
#include "net.h"
#include "openai.h"

namespace notes_sync {

static const char *TAG = "notes_sync";

// "Saturday, 2026-10-31, 11:15" from the memo's id "20261031-111508": the model
// counts "tomorrow" from when it was said, not from when it went online.
static std::string said_at(const std::string &id)
{
    static const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const int OFFSET[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int year, month, day, hour, minute;
    if (sscanf(id.c_str(), "%4d%2d%2d-%2d%2d", &year, &month, &day, &hour, &minute) != 5 || year < 2001 || month < 1 ||
        month > 12) {
        return "unknown";
    }
    const int y = year - (month < 3);
    const int weekday = (y + y / 4 - y / 100 + y / 400 + OFFSET[month - 1] + day) % 7;
    char s[48];
    snprintf(s, sizeof s, "%s, %04d-%02d-%02d, %02d:%02d", DAYS[weekday], year, month, day, hour, minute);
    return s;
}

Result run(void (*progress)(int index, int count))
{
    Result r;
    const auto ids = memos::waiting();
    r.left = (int)ids.size();
    if (ids.empty()) {
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

    for (size_t i = 0; i < ids.size(); i++) {
        if (progress != nullptr) {
            progress((int)i, (int)ids.size());
        }
        const std::string &id = ids[i];
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
        std::string note = text.empty() ? "" : openai::tidy(text, said_at(id).c_str());
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

}  // namespace notes_sync
