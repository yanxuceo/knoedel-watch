#include "net.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string.h>
#include <time.h>

#include "board.h"
#include "credentials.h"

namespace net {

static const char *TAG = "net";
static constexpr EventBits_t GOT_IP = 1, LOST = 2;
static EventGroupHandle_t events;
static bool started, up;
static char reached[33];

static void on_event(void *, esp_event_base_t base, int32_t id, void *)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        up = true;
        xEventGroupSetBits(events, GOT_IP);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        up = false;
        xEventGroupSetBits(events, LOST);
    }
}

static bool init()
{
    if (events != nullptr) {
        return true;
    }
    events = xEventGroupCreate();
    esp_netif_init();
    const esp_err_t loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) {
        return false;
    }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi did not start");
        return false;
    }
    esp_wifi_set_storage(WIFI_STORAGE_RAM);  // the networks live in credentials, not in the Wi-Fi's own NVS
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, nullptr);
    return true;
}

bool connect(uint32_t timeout_ms)
{
    if (up) {
        return true;
    }
    credentials::Network list[credentials::MAX_NETWORKS];
    const int n = credentials::networks(list, credentials::MAX_NETWORKS);
    if (n == 0) {
        ESP_LOGW(TAG, "no Wi-Fi network saved");
        return false;
    }
    if (!init()) {
        return false;
    }
    if (!started && esp_wifi_start() == ESP_OK) {
        started = true;
    }
    // The newest network first: it is most likely where the watch is now.
    for (int i = n - 1; i >= 0; i--) {
        // A 32-character name or 64-character password fills its field without a final zero.
        wifi_config_t wc = {};
        memcpy(wc.sta.ssid, list[i].ssid, strnlen(list[i].ssid, sizeof(wc.sta.ssid)));
        memcpy(wc.sta.password, list[i].password, strnlen(list[i].password, sizeof(wc.sta.password)));
        wc.sta.threshold.authmode = list[i].password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
        wc.sta.pmf_cfg.capable = true;
        esp_wifi_set_config(WIFI_IF_STA, &wc);
        xEventGroupClearBits(events, GOT_IP | LOST);
        const uint32_t t0 = xTaskGetTickCount();
        esp_wifi_connect();
        // A failed attempt is retried until the time is up (an access point may refuse the first one).
        while (pdTICKS_TO_MS(xTaskGetTickCount() - t0) < timeout_ms) {
            const EventBits_t bits = xEventGroupWaitBits(events, GOT_IP | LOST, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
            if (bits & GOT_IP) {
                strcpy(reached, list[i].ssid);
                ESP_LOGI(TAG, "online over \"%s\" in %lu ms", reached,
                         (unsigned long)pdTICKS_TO_MS(xTaskGetTickCount() - t0));
                return true;
            }
            if (bits & LOST) {
                vTaskDelay(pdMS_TO_TICKS(300));
                esp_wifi_connect();
            }
        }
        esp_wifi_disconnect();
        ESP_LOGW(TAG, "\"%s\" did not answer", list[i].ssid);
    }
    return false;
}

void disconnect()
{
    if (!started) {
        return;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    started = up = false;
}

bool connected() { return up; }

std::vector<Found> scan()
{
    std::vector<Found> found;
    if (!init()) {
        return found;
    }
    const bool was_started = started;
    if (!started && esp_wifi_start() == ESP_OK) {
        started = true;
    }
    wifi_scan_config_t cfg = {};
    if (esp_wifi_scan_start(&cfg, true) == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> aps(n);
        if (n > 0 && esp_wifi_scan_get_ap_records(&n, aps.data()) == ESP_OK) {
            for (uint16_t i = 0; i < n; i++) {
                const std::string ssid((const char *)aps[i].ssid);
                bool seen = ssid.empty();
                for (const Found &f : found) {
                    seen |= f.ssid == ssid;
                }
                if (!seen) {  // the records come strongest first
                    found.push_back({ssid, aps[i].rssi, aps[i].authmode != WIFI_AUTH_OPEN});
                }
            }
        }
    }
    if (!was_started && !up) {
        disconnect();
    }
    return found;
}

const char *network() { return reached; }

bool sync_time(uint32_t timeout_ms)
{
    if (!up) {
        return false;
    }
    // Started afresh each time: SNTP left running answers a second wait only at
    // its next round, an hour on, so a second sync in the same boot timed out.
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&cfg) != ESP_OK) {
        return false;
    }
    const bool synced = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
    esp_netif_sntp_deinit();
    if (!synced) {
        ESP_LOGW(TAG, "no time from the internet");
        return false;
    }
    // The watch's clock keeps local time.
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    const board::DateTime dt = {(int16_t)(t.tm_year + 1900), (int8_t)(t.tm_mon + 1), (int8_t)t.tm_mday,
                                (int8_t)t.tm_hour, (int8_t)t.tm_min, (int8_t)t.tm_sec};
    board::set_time(dt);
    ESP_LOGI(TAG, "clock set from the internet: %04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.month, dt.day, dt.hour,
             dt.minute, dt.second);
    return true;
}

}  // namespace net
