#pragma once
// Microphone test (CONFIG_KNOEDEL_MIC_TEST), see mic_test.cpp. Not for a release.

namespace mic_test {

// Records while the right key is held (up to 30 s), then sends the recording
// over USB. Leaves the screen to be redrawn by the caller.
void run();

}  // namespace mic_test
