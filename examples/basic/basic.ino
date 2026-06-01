#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <EspOta.h>

const char *SSID = "your-ssid";
const char *PASS = "your-password";

AsyncWebServer server(80);

void setup() {
    Serial.begin(115200);

    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.printf("Connected: %s\n", WiFi.localIP().toString().c_str());

    EspOta::init(server);
    server.begin();

    Serial.printf("OTA: http://%s/ota\n", WiFi.localIP().toString().c_str());
}

void loop() {
    if (EspOta::rebootPending()) {
        delay(500);
        ESP.restart();
    }
}
