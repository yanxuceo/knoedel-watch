#include "usb_commands.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/usb_serial_jtag_ll.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "credentials.h"
#include "memos.h"
#include "net.h"
#include "notes_sync.h"

namespace usb_commands {

// The console writes to the same port; reading its receive FIFO directly, with
// no driver installed, leaves the log as it is when no computer listens.

static void reply(const char *fmt, const char *arg = "")
{
    printf(fmt, arg);
    printf("\n");
    fflush(stdout);
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
        char names[credentials::MAX_NETWORKS * 36] = "";
        for (int i = 0; i < n; i++) {
            strcat(names, i ? ", \"" : "\"");
            strcat(names, list[i].ssid);
            strcat(names, "\"");
        }
        reply(n ? "OK %s" : "OK no networks", names);
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
    } else if (strcmp(line, "notes sync") == 0) {
        const notes_sync::Result r = notes_sync::run();
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
    static char line[320];
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

void start() { xTaskCreate(task, "usb_commands", 8192, nullptr, 2, nullptr); }

}  // namespace usb_commands
