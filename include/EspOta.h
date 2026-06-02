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
    // type-based (U_FLASH / U_SPIFFS):
    //   The Update library selects the correct partition automatically.
    //   U_FLASH always writes to the inactive OTA app slot.
    //   U_SPIFFS writes to the filesystem partition.
    //
    // address-based (type = U_UNKNOWN):
    //   Writes directly to the partition at the given flash address and size.
    //   Use this to target a specific partition by address (e.g. for two-program
    //   alternation or custom data partitions).
    struct OtaTarget {
        const char *label;    // URL key: POST /update/<label>
        const char *name;     // display name in the boot mode UI
        int         type;     // U_FLASH, U_SPIFFS, or U_UNKNOWN for address-based
        uint32_t    address;  // only used when type == U_UNKNOWN
        size_t      maxSize;  // only used when type == U_UNKNOWN
    };

    // Convenience constructors for common target types.
    inline OtaTarget flashTarget(const char *label, const char *name) {
        return {label, name, U_FLASH, 0, 0};
    }
    inline OtaTarget spiffsTarget(const char *label, const char *name) {
        return {label, name, U_SPIFFS, 0, 0};
    }
    inline OtaTarget addressTarget(const char *label, const char *name,
                                   uint32_t address, size_t maxSize) {
        return {label, name, U_UNKNOWN, address, maxSize};
    }

    // Default targets used when none are specified (firmware + filesystem).
    extern const OtaTarget DEFAULT_TARGETS[2];

    // Register OTA endpoints on the given server.
    // password    — if non-null, /ota requires a password-only login form.
    // targets     — upload targets to expose; defaults to firmware + filesystem.
    // targetCount — number of entries in targets.
    //
    // Boot mode behaviour:
    //   Entering /ota sets BOOT_MODE in NVS (survives reboot).
    //   GET / should redirect to /ota while isBootMode() is true.
    //   Boot mode clears only on /ota/exit, which also calls confirm().
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
