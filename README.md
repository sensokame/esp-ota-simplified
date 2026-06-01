# esp-ota-simplified

Simple OTA firmware and filesystem update handler for ESP32, built on [ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer).

Registers three endpoints on your existing server instance:

| Endpoint | Method | Description |
|---|---|---|
| `/ota` | GET | Built-in update UI (no LittleFS required) |
| `/update/firmware` | POST | Flash new firmware `.bin` |
| `/update/filesystem` | POST | Flash new filesystem image `.bin` |

## Installation

Add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/sensokame/esp-ota-simplified.git#v1.0.0
```

## Usage

```cpp
#include <ESPAsyncWebServer.h>
#include <EspOta.h>

AsyncWebServer server(80);

void setup() {
    // ... WiFi setup ...
    EspOta::init(server);          // no password
    // EspOta::init(server, "pw"); // with password
    server.begin();
}

void loop() {
    if (EspOta::rebootPending()) {
        delay(500);
        ESP.restart();
    }
}
```

Navigate to `http://<device-ip>/ota` to open the boot mode update page.

## Authentication

`/ota` and both upload endpoints can be protected with HTTP Basic Auth:

```cpp
EspOta::init(server, "mypassword");
```

The browser will prompt for credentials on the first visit. The username is always `ota`; only the password is configurable. Pass `nullptr` (or omit the argument) to disable protection.

> The boot mode page can flash arbitrary firmware — protect it if your device is reachable by untrusted clients on the same network.

## API

### `EspOta::init(AsyncWebServer &server)`
Registers the OTA endpoints on the provided server. Call before `server.begin()`.

### `EspOta::rebootPending()`
Returns `true` after a successful OTA upload. Check in `loop()` and reboot when true.

## Generating .bin files with PlatformIO

**Firmware:**
```bash
pio run
# → .pio/build/<env>/firmware.bin
```

**Filesystem:**
```bash
pio run --target buildfs
# → .pio/build/<env>/littlefs.bin
```

## Changelog

### v1.1.0
- Boot mode UI with amber styling, elapsed timer, and automatic reconnect polling after reboot
- Optional HTTP Basic Auth password via `EspOta::init(server, "password")`

### v1.0.0
- Initial release
- Firmware and filesystem OTA endpoints
- Built-in update UI served from PROGMEM (no LittleFS dependency)
