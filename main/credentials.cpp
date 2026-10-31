#include "credentials.h"

#include <nvs.h>
#include <stdio.h>
#include <string.h>

namespace credentials {

// NVS namespace "creds": "n" networks as s0/p0 .. s4/p4, oldest first; "openai" the key.
static const char *NS = "creds";

static bool open(nvs_open_mode_t mode, nvs_handle_t &h) { return nvs_open(NS, mode, &h) == ESP_OK; }

int networks(Network *out, int max)
{
    nvs_handle_t h;
    if (!open(NVS_READONLY, h)) {
        return 0;
    }
    uint8_t n = 0;
    nvs_get_u8(h, "n", &n);
    int count = 0;
    for (int i = 0; i < n && count < max; i++) {
        char key[8];
        size_t size = sizeof(out[count].ssid);
        snprintf(key, sizeof key, "s%d", i);
        if (nvs_get_str(h, key, out[count].ssid, &size) != ESP_OK) {
            continue;
        }
        size = sizeof(out[count].password);
        snprintf(key, sizeof key, "p%d", i);
        if (nvs_get_str(h, key, out[count].password, &size) != ESP_OK) {
            out[count].password[0] = '\0';
        }
        count++;
    }
    nvs_close(h);
    return count;
}

static bool write_networks(const Network *list, int n)
{
    nvs_handle_t h;
    if (!open(NVS_READWRITE, h)) {
        return false;
    }
    bool ok = true;
    for (int i = 0; i < MAX_NETWORKS; i++) {
        char s[8], p[8];
        snprintf(s, sizeof s, "s%d", i);
        snprintf(p, sizeof p, "p%d", i);
        if (i < n) {
            ok &= nvs_set_str(h, s, list[i].ssid) == ESP_OK && nvs_set_str(h, p, list[i].password) == ESP_OK;
        } else {
            nvs_erase_key(h, s);
            nvs_erase_key(h, p);
        }
    }
    ok &= nvs_set_u8(h, "n", (uint8_t)n) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool add_network(const char *ssid, const char *password)
{
    if (ssid[0] == '\0' || strlen(ssid) >= sizeof(Network::ssid) || strlen(password) >= sizeof(Network::password)) {
        return false;
    }
    Network list[MAX_NETWORKS + 1];
    int n = networks(list, MAX_NETWORKS);
    int at = n;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].ssid, ssid) == 0) {
            at = i;
        }
    }
    if (at == n) {
        n++;
    }
    strcpy(list[at].ssid, ssid);
    strcpy(list[at].password, password);
    if (n > MAX_NETWORKS) {  // the oldest one goes
        memmove(list, list + 1, sizeof(Network) * MAX_NETWORKS);
        n = MAX_NETWORKS;
    }
    return write_networks(list, n);
}

void clear_networks() { write_networks(nullptr, 0); }

bool openai_key(char *out, size_t size)
{
    nvs_handle_t h;
    if (!open(NVS_READONLY, h)) {
        return false;
    }
    const bool ok = nvs_get_str(h, "openai", out, &size) == ESP_OK && out[0] != '\0';
    nvs_close(h);
    return ok;
}

bool has_key()
{
    char key[256];
    return openai_key(key, sizeof key);
}

void key_hint(char *out, size_t size)
{
    char key[256];
    if (!openai_key(key, sizeof key)) {
        snprintf(out, size, "none");
        return;
    }
    const size_t n = strlen(key);
    snprintf(out, size, "sk-...%s (%u characters)", n > 4 ? key + n - 4 : "", (unsigned)n);
}

bool set_openai_key(const char *key)
{
    nvs_handle_t h;
    if (!open(NVS_READWRITE, h)) {
        return false;
    }
    const bool ok = nvs_set_str(h, "openai", key) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

}  // namespace credentials
