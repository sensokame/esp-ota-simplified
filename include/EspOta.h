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
    //
    // type = U_FLASH, partitionLabel = nullptr:
    //   Writes to the inactive OTA app slot (standard alternating OTA behaviour).
    //
    // type = U_FLASH, partitionLabel = "app0" / "app1" / ...:
    //   Writes to the named partition from the partition table CSV.
    //   Use this to target a specific slot (e.g. two-program alternation).
    //
    // type = U_SPIFFS, partitionLabel = nullptr:
    //   Writes to the filesystem partition.
    struct OtaTarget {
        const char *label;          // URL key: POST /update/<label>
        const char *name;           // display name in the boot mode UI
        int         type;           // U_FLASH or U_SPIFFS
        const char *partitionLabel; // nullptr = automatic; or CSV partition name
    };

    // Convenience constructors.
    inline OtaTarget flashTarget(const char *label, const char *name) {
        return {label, name, U_FLASH, nullptr};
    }
    inline OtaTarget spiffsTarget(const char *label, const char *name) {
        return {label, name, U_SPIFFS, nullptr};
    }
    // Target a specific partition by its name in the CSV (e.g. "app0", "app1").
    inline OtaTarget partitionTarget(const char *label, const char *name,
                                     const char *partitionLabel) {
        return {label, name, U_FLASH, partitionLabel};
    }

    // Default targets used when none are specified (firmware + filesystem).
    extern const OtaTarget DEFAULT_TARGETS[2];

    // Register OTA endpoints on the given server.
    // password    — if non-null, /ota requires a password-only login form.
    // targets     — upload targets to expose; defaults to firmware + filesystem.
    // targetCount — number of entries in targets.
    //
    // Boot mode: entering /ota sets BOOT_MODE in NVS (survives reboot).
    // GET / should redirect to /ota while isBootMode() is true.
    // Boot mode clears only on /ota/exit, which also calls confirm().
    void init(AsyncWebServer &server,
              const char *password = nullptr,
              const OtaTarget *targets = DEFAULT_TARGETS,
              uint8_t targetCount = 2);

    State state();
    bool  isBootMode();    // state() == BOOT_MODE
    bool  rebootPending(); // state() == REBOOTING

    // Mark the running firmware as valid, cancelling any pending OTA rollback.
    // Called automatically on /ota/exit. Available for manual use.
    void confirm();
}
