#pragma once
#include <ESPAsyncWebServer.h>
#include <Update.h>

namespace EspOta {

    enum class State : uint8_t {
        NORMAL,    // device running normally
        BOOT_MODE, // boot mode active, awaiting upload or exit
        FLASHING,  // OTA upload in progress
        REBOOTING  // upload complete, pending ESP.restart()
    };

    // Describes one uploadable target shown in the boot mode UI.
    // label  — used in the URL: POST /update/<label>
    // name   — display name shown in the UI card header
    // type   — Update library partition type (U_FLASH or U_SPIFFS)
    struct OtaTarget {
        const char *label;
        const char *name;
        int         type;
    };

    // Default targets used when none are specified.
    extern const OtaTarget DEFAULT_TARGETS[2];

    // Register OTA endpoints on the given server.
    // password   — if non-null, /ota requires a password-only login form.
    // targets    — array of upload targets to expose; defaults to firmware + filesystem.
    // targetCount — number of entries in targets.
    //
    // Boot mode behaviour:
    //   On first authenticated access to /ota the device enters BOOT_MODE (persisted
    //   in NVS). GET / should redirect to /ota while isBootMode() is true.
    //   Boot mode is cleared only when the user exits via /ota/exit, which also
    //   calls confirm(). On reboot after OTA the device restores BOOT_MODE from NVS.
    void init(AsyncWebServer &server,
              const char *password = nullptr,
              const OtaTarget *targets = DEFAULT_TARGETS,
              uint8_t targetCount = 2);

    // Current OTA state.
    State state();

    // Convenience wrappers — use these in application code.
    bool isBootMode();    // state() == BOOT_MODE
    bool rebootPending(); // state() == REBOOTING

    // Mark the running firmware as valid, cancelling any pending OTA rollback.
    // Called automatically when the user exits boot mode. Available for manual use.
    void confirm();
}
