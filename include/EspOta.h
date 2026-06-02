#pragma once
#include <ESPAsyncWebServer.h>

namespace EspOta {
    // Register OTA endpoints on the given server.
    // Pass a non-null password to protect /ota with a password-only login form.
    // On first access to /ota the device enters boot mode (persisted in NVS):
    //   - GET / redirects to /ota while boot mode is active
    //   - Boot mode is cleared only when the user explicitly exits via /ota/exit
    //   - Exiting calls confirm() automatically
    void init(AsyncWebServer &server, const char *password = nullptr);

    // Returns true once an OTA upload finishes successfully. Check in loop() and reboot.
    bool rebootPending();

    // Returns true while the device is in boot mode.
    // Use this in your GET / handler to redirect to /ota.
    bool isBootMode();

    // Mark the running firmware as valid, cancelling any pending OTA rollback.
    // Called automatically on exit from boot mode. Available for manual use if needed.
    void confirm();
}
