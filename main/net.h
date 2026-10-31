#pragma once
// Wi-Fi for the notes: on only while something has to go out, off again after.

#include <stdint.h>

namespace net {

// Tries the saved networks (credentials.h) one after the other, until one
// gives the watch an address. False when none does within `timeout_ms` each.
bool connect(uint32_t timeout_ms = 10000);

// Wi-Fi off.
void disconnect();

bool connected();

// The name of the network connect() reached.
const char *network();

// Sets the watch's clock from the internet, in German time (needs connect()).
bool sync_time(uint32_t timeout_ms = 6000);

}  // namespace net
