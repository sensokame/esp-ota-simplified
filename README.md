# esp-ota-simplified

Simple OTA firmware update handler for ESP32, built on [ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer).

## Features

- **Boot mode UI** at `GET /ota` — amber-styled page served from PROGMEM (no LittleFS needed)
- **Password-only login** — cookie-based session, no username required
- **Boot mode persistence** — after a completed OTA upload, device reboots back into boot mode for verification; manual restart always returns to NORMAL
- **OTA rollback** — new firmware must call `confirm()` (via exit) or it rolls back on next reboot
- **Configurable upload targets** — any number of partitions by type or label
- **State machine** — `NORMAL → BOOT_MODE → FLASHING → REBOOTING`

## Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/ota` | GET | Login form (if password set) or boot mode UI |
| `/ota/login` | POST | Authenticate, set session cookie |
| `/ota/exit` | GET | Exit boot mode, confirm firmware, redirect to `/` |
| `/ota/targets` | GET | JSON list of configured upload targets |
| `/update/<label>` | POST | Upload binary for the named target |

## Installation

```ini
lib_deps =
    https://github.com/sensokame/esp-ota-simplified.git#v1.1.0
```

## Usage

```cpp
#include <ESPAsyncWebServer.h>
#include <EspOta.h>

AsyncWebServer server(80);

void setup() {
    // ... WiFi setup ...

    // Default targets: firmware (U_FLASH) + filesystem (U_SPIFFS)
    EspOta::init(server, "mypassword");
    server.begin();
}

void loop() {
    if (EspOta::rebootPending()) {
        delay(500);
        ESP.restart();
    }
}
```

Navigate to `http://<device-ip>/ota` to enter boot mode.

## Custom targets

```cpp
// Firmware only (no filesystem partition)
static const EspOta::OtaTarget targets[] = {
    EspOta::flashTarget("firmware", "Firmware"),
};
EspOta::init(server, "mypassword", targets, 1);

// Two named app slots (e.g. for alternating firmware)
static const EspOta::OtaTarget targets[] = {
    EspOta::partitionTarget("slot-a", "App Slot A", "app0"),
    EspOta::partitionTarget("slot-b", "App Slot B", "app1"),
};
EspOta::init(server, "mypassword", targets, 2);
```

### Target constructors

```cpp
EspOta::flashTarget("firmware", "Firmware")
// → U_FLASH, auto-selects inactive OTA slot

EspOta::spiffsTarget("filesystem", "Filesystem")
// → U_SPIFFS, targets filesystem partition

EspOta::partitionTarget("slot-a", "App Slot A", "app0")
// → U_FLASH targeting partition labelled "app0" in partitions.csv
```

## API

### `EspOta::init(server, password, targets, targetCount)`
Registers all OTA endpoints. `password` and `targets` are optional (defaults: no password, firmware + filesystem).

### `EspOta::state()`
Returns current `EspOta::State`: `NORMAL`, `BOOT_MODE`, `FLASHING`, or `REBOOTING`.

### `EspOta::isBootMode()`
Returns `true` while in BOOT_MODE. Use in `GET /` to redirect to `/ota`.

```cpp
server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (EspOta::isBootMode()) {
        AsyncWebServerResponse *r = req->beginResponse(302, "text/plain", "");
        r->addHeader("Location", "/ota");
        r->addHeader("Cache-Control", "no-store");
        req->send(r);
        return;
    }
    // serve normal UI
});
```

### `EspOta::rebootPending()`
Returns `true` after a successful upload. Check in `loop()` and call `ESP.restart()`.

### `EspOta::confirm()`
Marks firmware as valid, cancelling OTA rollback. Called automatically on `/ota/exit`. Available for manual use.

## Authentication

`/ota` and the upload endpoints use cookie-based session auth. On login the browser receives a session cookie (`Path=/ota; HttpOnly`); no username is required.

```cpp
EspOta::init(server, "mypassword"); // protected
EspOta::init(server);              // no protection
```

`/ota/exit` is always accessible without auth — exiting boot mode is safe.

## OTA rollback behaviour

The ESP32 has two OTA app partitions. After an upload the device reboots into new firmware in an unconfirmed state. If the user exits boot mode (`/ota/exit`), `confirm()` is called and the firmware is permanent. If the device reboots before exiting, it rolls back to the previous firmware.

Boot mode is persisted to NVS **only when an upload completes** — visiting `/ota` without uploading does not affect reboot behaviour.

## Route ordering note

ESPAsyncWebServer does prefix matching — a handler for `/ota` will also match `/ota/exit`. Always register specific sub-routes (`/ota/exit`, `/ota/login`, `/ota/targets`) **before** the generic `/ota` handler. This library handles this internally.

## Generating .bin files

```bash
pio run                       # firmware → .pio/build/<env>/firmware.bin
pio run --target buildfs      # filesystem → .pio/build/<env>/littlefs.bin
```

## Changelog

### v1.1.0
- State machine: `NORMAL / BOOT_MODE / FLASHING / REBOOTING`
- Cookie-based session auth (password only, no username)
- Boot mode persisted to NVS only on completed upload; manual restart always returns to NORMAL
- Configurable N upload targets via `OtaTarget` struct and convenience constructors
- `EspOta::confirm()` for OTA rollback support
- Fixed route registration order (prefix match issue with `/ota/*`)
- Boot mode UI: dynamic target cards fetched from `/ota/targets`

### v1.0.0
- Initial release: firmware + filesystem OTA endpoints, built-in UI from PROGMEM
