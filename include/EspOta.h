#pragma once
#include <ESPAsyncWebServer.h>

namespace EspOta {
    // Register /ota (boot mode UI), /update/firmware and /update/filesystem.
    // Pass a non-null password to protect all three with HTTP Basic Auth.
    void init(AsyncWebServer &server, const char *password = nullptr);

    // Returns true once an OTA update finishes. Check in loop() and reboot.
    bool rebootPending();

    // Mark the running firmware as valid, cancelling any pending rollback.
    // Call once at the end of setup() after WiFi and server are confirmed working.
    // If this is never called after an OTA update, the device rolls back to the
    // previous firmware on the next reboot.
    void confirm();
}
