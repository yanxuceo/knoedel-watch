#include "usb_commands.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/usb_serial_jtag_ll.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <cJSON.h>
#include <esp_app_desc.h>

#include "board.h"
#include "credentials.h"
#include "memos.h"
#include "net.h"
#include "notes_sync.h"
#include "openai.h"
#include "sentences.h"

namespace usb_commands {

// The console writes to the same port; reading its receive FIFO directly, with
// no driver installed, leaves the log as it is when no computer listens.

static void reply(const char *fmt, const char *arg = "")
{
    printf(fmt, arg);
    printf("\n");
    fflush(stdout);
}

// "OK " and the JSON on one line.
static void reply_json(cJSON *json)
{
    char *text = cJSON_PrintUnformatted(json);
    printf("OK %s\n", text);
    fflush(stdout);
    cJSON_free(text);
    cJSON_Delete(json);
}

static void handle(char *line)
{
    if (strncmp(line, "wifi add ", 9) == 0) {
        char *ssid = line + 9, *tab = strchr(ssid, '\t');
        const char *password = "";
        if (tab != nullptr) {
            *tab = '\0';
            password = tab + 1;
        }
        reply(credentials::add_network(ssid, password) ? "OK saved \"%s\"" : "ERR could not save \"%s\"", ssid);
    } else if (strcmp(line, "wifi list") == 0) {
        credentials::Network list[credentials::MAX_NETWORKS];
        const int n = credentials::networks(list, credentials::MAX_NETWORKS);
        cJSON *names = cJSON_CreateArray();
        for (int i = 0; i < n; i++) {
            cJSON_AddItemToArray(names, cJSON_CreateString(list[i].ssid));
        }
        reply_json(names);
    } else if (strncmp(line, "wifi remove ", 12) == 0) {
        reply(credentials::remove_network(line + 12) ? "OK removed \"%s\"" : "ERR no network \"%s\"", line + 12);
    } else if (strcmp(line, "wifi scan") == 0) {
        if (notes_sync::busy()) {
            reply("ERR busy sending notes, try again in a moment");
        } else {
            cJSON *found = cJSON_CreateArray();
            for (const net::Found &f : net::scan()) {
                cJSON *o = cJSON_CreateObject();
                cJSON_AddStringToObject(o, "ssid", f.ssid.c_str());
                cJSON_AddNumberToObject(o, "rssi", f.rssi);
                cJSON_AddBoolToObject(o, "secure", f.secure);
                cJSON_AddItemToArray(found, o);
            }
            reply_json(found);
        }
    } else if (strcmp(line, "wifi clear") == 0) {
        credentials::clear_networks();
        reply("OK forgot all networks");
    } else if (strncmp(line, "key set ", 8) == 0) {
        const char *key = line + 8;
        const size_t n = strlen(key);
        if (strncmp(key, "sk-", 3) != 0 || n < 20 || n > 250) {
            reply("ERR that does not look like an OpenAI key");
        } else {
            reply(credentials::set_openai_key(key) ? "OK key saved" : "ERR could not save the key");
        }
    } else if (strcmp(line, "key show") == 0) {
        char hint[64];
        credentials::key_hint(hint, sizeof hint);
        reply("OK %s", hint);
    } else if (strcmp(line, "net test") == 0) {
        const bool online = net::connect();
        const bool timed = online && net::sync_time();
        char text[96];
        snprintf(text, sizeof text, "\"%s\", clock %s", net::network(), timed ? "set" : "not set");
        net::disconnect();
        reply(online ? "OK online over %s" : "ERR no saved network answered", online ? text : "");
    } else if (strcmp(line, "info") == 0) {
        size_t used = 0, total = 0;
        memos::usage(used, total);
        char hint[64];
        credentials::key_hint(hint, sizeof hint);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "version", esp_app_get_description()->version);
        cJSON_AddNumberToObject(o, "battery", board::battery_level());
        cJSON_AddBoolToObject(o, "usb", board::on_usb());
        cJSON_AddNumberToObject(o, "notes", (double)memos::notes().size());
        cJSON_AddNumberToObject(o, "waiting", (double)memos::waiting().size());
        cJSON_AddNumberToObject(o, "used_kb", (double)(used / 1024));
        cJSON_AddNumberToObject(o, "total_kb", (double)(total / 1024));
        cJSON_AddStringToObject(o, "key", credentials::has_key() ? hint : "");
        cJSON_AddStringToObject(o, "mode", notes_sync::saving() ? "saving" : "now");
        reply_json(o);
    } else if (strcmp(line, "key test") == 0) {
        if (!credentials::has_key()) {
            reply("ERR no key on the watch");
        } else if (notes_sync::busy()) {
            reply("ERR busy sending notes, try again in a moment");
        } else if (!net::connect()) {
            reply("ERR no saved network answered");
        } else {
            const int status = openai::check_key();
            net::disconnect();
            char text[64];
            snprintf(text, sizeof text, "%d", status);
            reply(status == 200   ? "OK the key works (HTTP %s)"
                  : status == 401 ? "ERR the key was refused (HTTP %s)"
                  : status == 429 ? "ERR the monthly limit is reached, or too many requests (HTTP %s)"
                  : status == 0   ? "ERR no answer from OpenAI%s"
                                  : "ERR OpenAI answered HTTP %s",
                  status == 0 ? "" : text);
        }
    } else if (strcmp(line, "words get") == 0) {
        cJSON *w = cJSON_CreateString(credentials::words().c_str());
        reply_json(w);
    } else if (strncmp(line, "words set ", 10) == 0) {
        // A JSON string, so that the lines can come in one line.
        cJSON *w = cJSON_Parse(line + 10);
        const bool ok = cJSON_IsString(w) && credentials::set_words(w->valuestring);
        cJSON_Delete(w);
        reply(ok ? "OK words saved" : "ERR the words are too long or not a JSON string");
    } else if (strcmp(line, "mode get") == 0) {
        reply(notes_sync::saving() ? "OK saving" : "OK now");
    } else if (strcmp(line, "mode set saving") == 0 || strcmp(line, "mode set now") == 0) {
        notes_sync::set_saving(strcmp(line, "mode set saving") == 0);
        reply("OK mode saved");
    } else if (strcmp(line, "sent list") == 0) {
        const auto list = sentences::all();
        for (const sentences::Sentence &s : list) {
            printf("SENT %s\n", sentences::to_json(s).c_str());
        }
        char text[24];
        snprintf(text, sizeof text, "%u", (unsigned)list.size());
        reply("OK %s", text);
    } else if (strncmp(line, "sent add ", 9) == 0) {
        const std::string id = sentences::add(line + 9);
        reply(id.empty() ? "ERR not a sentence: \"de\" and \"zh\" or \"en\" are needed%s" : "OK %s", id.c_str());
    } else if (strncmp(line, "sent update ", 12) == 0) {
        reply(sentences::update(line + 12) ? "OK saved" : "ERR no sentence with that id, or one without German");
    } else if (strncmp(line, "sent remove ", 12) == 0) {
        reply(sentences::remove(line + 12) ? "OK removed" : "ERR no sentence %s", line + 12);
    } else if (strcmp(line, "sent clear") == 0) {
        sentences::clear();
        reply("OK all sentences deleted");
    } else if (strcmp(line, "notes list") == 0) {
        const auto ids = memos::notes();
        for (const auto &id : ids) {
            printf("NOTE %s\n", memos::note(id).c_str());
        }
        size_t used = 0, total = 0;
        memos::usage(used, total);
        char text[96];
        snprintf(text, sizeof text, "%u notes, %u memos waiting, %u of %u KB used", (unsigned)ids.size(),
                 (unsigned)memos::waiting().size(), (unsigned)(used / 1024), (unsigned)(total / 1024));
        reply("OK %s", text);
    } else if (strcmp(line, "notes sync") == 0 && memos::waiting().empty()) {
        reply("OK no memos waiting");
    } else if (strcmp(line, "notes sync") == 0) {
        // Asks the background task and waits for its next go to finish.
        uint32_t before = 0, now = 0;
        notes_sync::last(&before);
        notes_sync::request();
        for (int i = 0; i < 3000 && (notes_sync::last(&now), now == before); i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        const notes_sync::Result r = notes_sync::last();
        char text[96];
        snprintf(text, sizeof text, "%d done, %d left%s%s", r.done, r.left, r.offline ? ", offline" : "",
                 r.limited ? ", monthly limit reached" : "");
        reply("OK %s", text);
    } else if (strcmp(line, "notes clear") == 0) {
        for (const auto &id : memos::notes()) {
            unlink(("/notes/done/" + id + ".json").c_str());
        }
        reply("OK notes deleted");
    } else if (line[0] != '\0') {
        reply("ERR unknown command");
    }
}

static void task(void *)
{
    static char line[1200];  // the personal words come in one line
    size_t n = 0;
    while (true) {
        uint8_t buf[64];
        const uint32_t got = usb_serial_jtag_ll_rxfifo_data_available() ? usb_serial_jtag_ll_read_rxfifo(buf, sizeof buf) : 0;
        for (uint32_t i = 0; i < got; i++) {
            if (buf[i] == '\n' || buf[i] == '\r') {
                line[n] = '\0';
                handle(line);
                memset(line, 0, sizeof line);  // a key does not stay in memory
                n = 0;
            } else if (n + 1 < sizeof line) {
                line[n++] = (char)buf[i];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(got ? 1 : 30));
    }
}

// 16 KB: "key test" does TLS in this task.
void start() { xTaskCreate(task, "usb_commands", 16384, nullptr, 2, nullptr); }

}  // namespace usb_commands
