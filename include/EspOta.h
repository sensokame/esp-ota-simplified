#pragma once
#include <ESPAsyncWebServer.h>

namespace EspOta {
    // Register /ota (boot mode UI), /update/firmware and /update/filesystem.
    // Pass a non-null password to protect all three with HTTP Basic Auth.
    void init(AsyncWebServer &server, const char *password = nullptr);

    // Returns true once an OTA update finishes. Check in loop() and reboot.
    bool rebootPending();
}
