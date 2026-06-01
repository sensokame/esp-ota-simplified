#include "EspOta.h"
#include <Update.h>
#include <Arduino.h>

static bool _rebootPending = false;

static const char OTA_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OTA Update</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #0d0d0d; color: #e0e0e0; font-family: monospace; padding: 24px; max-width: 480px; margin: 0 auto; }
h1 { color: #00c8ff; font-size: 1.4rem; letter-spacing: 3px; margin-bottom: 28px; }
.card { background: #141414; border: 1px solid #222; border-radius: 6px; padding: 20px; margin-bottom: 14px; }
.card h2 { font-size: 0.65rem; letter-spacing: 3px; color: #444; text-transform: uppercase; margin-bottom: 14px; }
.file-label { display: block; background: #0d0d0d; border: 1px dashed #2a2a2a; color: #444; padding: 18px; border-radius: 4px; text-align: center; cursor: pointer; margin-bottom: 10px; font-size: 0.85rem; }
.file-label:hover { border-color: #555; color: #aaa; }
.file-label.selected { border-color: #00c8ff; color: #00c8ff; border-style: solid; }
input[type=file] { display: none; }
button { width: 100%; padding: 10px; border: 1px solid #2a2a2a; border-radius: 4px; background: #1e1e1e; color: #aaa; font-family: monospace; font-size: 0.85rem; cursor: pointer; }
button:hover:not(:disabled) { border-color: #00c8ff; color: #00c8ff; }
button:disabled { opacity: 0.4; cursor: not-allowed; }
.progress-wrap { margin-top: 12px; display: none; }
.progress-bar { background: #1e1e1e; border-radius: 3px; height: 4px; overflow: hidden; margin-bottom: 8px; }
.progress-fill { background: #00c8ff; height: 100%; width: 0; transition: width 0.2s; }
.status { font-size: 0.78rem; color: #555; }
.status.ok { color: #00c8ff; }
.status.error { color: #ff4444; }
</style>
</head>
<body>
<h1>OTA UPDATE</h1>

<div class="card">
  <h2>Firmware</h2>
  <label class="file-label" for="fw-file" id="fw-label">click to select firmware .bin</label>
  <input type="file" id="fw-file" accept=".bin" onchange="sel('fw')">
  <button onclick="upload('fw')" id="fw-btn">Upload firmware</button>
  <div class="progress-wrap" id="fw-wrap">
    <div class="progress-bar"><div class="progress-fill" id="fw-fill"></div></div>
    <div class="status" id="fw-status"></div>
  </div>
</div>

<div class="card">
  <h2>Filesystem</h2>
  <label class="file-label" for="fs-file" id="fs-label">click to select filesystem .bin</label>
  <input type="file" id="fs-file" accept=".bin" onchange="sel('fs')">
  <button onclick="upload('fs')" id="fs-btn">Upload filesystem</button>
  <div class="progress-wrap" id="fs-wrap">
    <div class="progress-bar"><div class="progress-fill" id="fs-fill"></div></div>
    <div class="status" id="fs-status"></div>
  </div>
</div>

<script>
function sel(t) {
  const f = document.getElementById(t+'-file').files[0];
  if (f) { const l = document.getElementById(t+'-label'); l.textContent = f.name; l.classList.add('selected'); }
}
function upload(t) {
  const file = document.getElementById(t+'-file').files[0];
  if (!file) return;
  const endpoint = t === 'fw' ? '/update/firmware' : '/update/filesystem';
  const wrap = document.getElementById(t+'-wrap');
  const fill = document.getElementById(t+'-fill');
  const st = document.getElementById(t+'-status');
  const btn = document.getElementById(t+'-btn');
  wrap.style.display = 'block';
  btn.disabled = true;
  st.className = 'status';
  st.textContent = 'uploading...';
  let done = false;
  const xhr = new XMLHttpRequest();
  xhr.open('POST', endpoint);
  xhr.upload.onprogress = e => {
    if (e.lengthComputable) {
      const p = Math.round(e.loaded/e.total*100);
      fill.style.width = p+'%';
      st.textContent = 'uploading... '+p+'%';
      if (p === 100) done = true;
    }
  };
  xhr.onload = () => {
    if (xhr.status === 200) { st.className='status ok'; st.textContent='done — rebooting...'; }
    else { st.className='status error'; st.textContent='upload failed'; btn.disabled=false; }
  };
  xhr.onerror = () => {
    if (done) { st.className='status ok'; st.textContent='done — rebooting...'; }
    else { st.className='status error'; st.textContent='connection lost'; btn.disabled=false; }
  };
  const form = new FormData();
  form.append('file', file);
  xhr.send(form);
}
</script>
</body>
</html>
)html";

static void handleUpload(AsyncWebServerRequest *req, String filename,
                         size_t index, uint8_t *data, size_t len, bool final,
                         int type) {
    if (!index) {
        Serial.printf("# OTA start: %s\n", filename.c_str());
        Update.begin(UPDATE_SIZE_UNKNOWN, type);
    }
    Update.write(data, len);
    if (final) {
        if (Update.end(true)) {
            Serial.printf("# OTA done: %u bytes\n", index + len);
            _rebootPending = true;
        } else {
            Serial.println("# OTA failed");
        }
    }
}

static void sendResult(AsyncWebServerRequest *req) {
    bool ok = !Update.hasError();
    req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"flash failed\"}");
}

namespace EspOta {

void init(AsyncWebServer &server) {
    server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send_P(200, "text/html", OTA_HTML);
    });

    server.on("/update/firmware", HTTP_POST,
        sendResult,
        [](AsyncWebServerRequest *req, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {
            handleUpload(req, filename, index, data, len, final, U_FLASH);
        }
    );

    server.on("/update/filesystem", HTTP_POST,
        sendResult,
        [](AsyncWebServerRequest *req, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {
            handleUpload(req, filename, index, data, len, final, U_SPIFFS);
        }
    );
}

bool rebootPending() { return _rebootPending; }

}
