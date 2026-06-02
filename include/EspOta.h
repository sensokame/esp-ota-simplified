#pragma once
#include <ESPAsyncWebServer.h>

namespace EspOta {

    enum class State : uint8_t {
        NORMAL,    // device running normally
        BOOT_MODE, // boot mode active, awaiting upload or exit
        FLASHING,  // OTA upload in progress
        REBOOTING  // upload complete, pending ESP.restart()
    };

    // Register OTA endpoints on the given server.
    // Pass a non-null password to protect /ota with a password-only login form.
    // On first authenticated access to /ota the device enters BOOT_MODE (persisted
    // in NVS). Boot mode is cleared only when the user exits via /ota/exit, which
    // also calls confirm(). While in BOOT_MODE, GET / should redirect to /ota.
    void init(AsyncWebServer &server, const char *password = nullptr);

    // Current OTA state.
    State state();

    // Convenience wrappers — use these in application code.
    bool isBootMode();    // state() == BOOT_MODE
    bool rebootPending(); // state() == REBOOTING

    // Mark the running firmware as valid, cancelling any pending OTA rollback.
    // Called automatically when the user exits boot mode. Available for manual use.
    void confirm();
}
