#pragma once
// What the watch needs to go online for the notes: the Wi-Fi networks it may
// use and the OpenAI API key, kept in NVS. They come in over USB
// (usb_commands.h) and never go back out: the key shows only as its last four
// characters, and none of it is written to the log.

#include <stddef.h>

namespace credentials {

static constexpr int MAX_NETWORKS = 5;

struct Network {
    char ssid[33];
    char password[65];
};

// The saved networks, oldest first.
int networks(Network *out, int max);
// Adds a network, or changes the password of one with that name. When all
// places are taken, the oldest one goes.
bool add_network(const char *ssid, const char *password);
void clear_networks();

bool has_key();
// The whole key, for the requests only.
bool openai_key(char *out, size_t size);
// "sk-…" plus the last four characters, or "none".
void key_hint(char *out, size_t size);
bool set_openai_key(const char *key);

}  // namespace credentials
