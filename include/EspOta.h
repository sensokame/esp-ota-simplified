#pragma once
#include <ESPAsyncWebServer.h>

namespace EspOta {
    // Register /ota (UI page), /update/firmware and /update/filesystem on the given server.
    void init(AsyncWebServer &server);

    // Returns true once an OTA update finishes. Check in loop() and reboot.
    bool rebootPending();
}
