#include "EspOta.h"
#include <Update.h>
#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_random.h>

static bool    _rebootPending = false;
static bool    _bootMode      = false;
static String  _password;
static String  _sessionToken;

// ── NVS ──────────────────────────────────────────────────────────────────────

static void loadBootMode() {
    Preferences p;
    p.begin("ota-lib", true);
    _bootMode = p.getBool("boot", false);
    p.end();
}

static void saveBootMode(bool mode) {
    Preferences p;
    p.begin("ota-lib", false);
    p.putBool("boot", mode);
    p.end();
    _bootMode = mode;
}

// ── Session ───────────────────────────────────────────────────────────────────

static bool hasSession(AsyncWebServerRequest *req) {
    if (_password.isEmpty()) return true;
    String cookie = req->header("Cookie");
    return _sessionToken.length() > 0 &&
           cookie.indexOf("session=" + _sessionToken) >= 0;
}

static void applySessionCookie(AsyncWebServerResponse *resp) {
    resp->addHeader("Set-Cookie",
        "session=" + _sessionToken + "; Path=/ota; HttpOnly");
}

static void clearSessionCookie(AsyncWebServerResponse *resp) {
    resp->addHeader("Set-Cookie",
        "session=; Path=/ota; HttpOnly; Max-Age=0");
}

// ── HTML ──────────────────────────────────────────────────────────────────────

static const char LOGIN_HTML[] PROGMEM = R"html(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Boot Mode</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0a0a;color:#e0e0e0;font-family:monospace;display:flex;align-items:center;justify-content:center;min-height:100vh}
.box{text-align:center;padding:40px}
.badge{background:#ff9500;color:#000;font-size:.7rem;font-weight:700;letter-spacing:3px;padding:4px 10px;border-radius:3px;display:inline-block;margin-bottom:32px}
input[type=password]{background:#141414;border:1px solid #2a2a2a;color:#e0e0e0;padding:12px 16px;border-radius:4px;font-family:monospace;font-size:.9rem;width:240px;display:block;margin:0 auto 12px;text-align:center}
input:focus{outline:none;border-color:#ff9500}
button{padding:10px 0;border:1px solid #ff9500;border-radius:4px;background:transparent;color:#ff9500;font-family:monospace;font-size:.85rem;cursor:pointer;width:240px}
button:hover{background:#ff9500;color:#000}
</style></head>
<body><div class="box">
<div class="badge">BOOT MODE</div><br><br>
<form method="POST" action="/ota/login">
<input type="password" name="p" placeholder="password" autofocus><br>
<button type="submit">Enter</button>
</form>
</div></body></html>
)html";

static const char OTA_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Boot Mode</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #0a0a0a; color: #e0e0e0; font-family: monospace; padding: 24px; max-width: 480px; margin: 0 auto; }
.topbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 28px; }
.badge { background: #ff9500; color: #000; font-size: 0.7rem; font-weight: bold; letter-spacing: 3px; padding: 4px 10px; border-radius: 3px; }
.exit-link { color: #555; font-size: 0.8rem; text-decoration: none; }
.exit-link:hover { color: #ff9500; }
h1 { color: #ff9500; font-size: 1.4rem; letter-spacing: 3px; margin-bottom: 24px; }
.card { background: #141414; border: 1px solid #2a2a2a; border-radius: 6px; padding: 20px; margin-bottom: 14px; }
.card h2 { font-size: 0.65rem; letter-spacing: 3px; color: #444; text-transform: uppercase; margin-bottom: 14px; }
.file-label { display: block; background: #0a0a0a; border: 1px dashed #2a2a2a; color: #444; padding: 18px; border-radius: 4px; text-align: center; cursor: pointer; margin-bottom: 10px; font-size: 0.85rem; }
.file-label:hover { border-color: #555; color: #888; }
.file-label.selected { border-color: #ff9500; color: #ff9500; border-style: solid; }
input[type=file] { display: none; }
button { width: 100%; padding: 10px; border: 1px solid #2a2a2a; border-radius: 4px; background: #1a1a1a; color: #888; font-family: monospace; font-size: 0.85rem; cursor: pointer; }
button:hover:not(:disabled) { border-color: #ff9500; color: #ff9500; }
button:disabled { opacity: 0.4; cursor: not-allowed; }
.progress-wrap { margin-top: 14px; display: none; }
.progress-bar { background: #1a1a1a; border-radius: 3px; height: 4px; overflow: hidden; margin-bottom: 10px; }
.progress-fill { background: #ff9500; height: 100%; width: 0; transition: width 0.2s; }
.status { font-size: 0.8rem; color: #555; }
.status.ok { color: #ff9500; }
.status.error { color: #ff4444; }
.reconnect { display: none; text-align: center; padding: 24px; }
.reconnect p { color: #555; font-size: 0.85rem; margin-bottom: 6px; }
.reconnect .count { color: #ff9500; font-size: 1.4rem; letter-spacing: 2px; }
</style>
</head>
<body>

<div class="topbar">
  <span class="badge">BOOT MODE</span>
  <a href="/ota/exit" class="exit-link">exit boot mode &#8594;</a>
</div>

<h1>UPDATE</h1>

<div id="main">
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
</div>

<div class="reconnect" id="reconnect">
  <p>Update complete. Rebooting...</p>
  <div class="count" id="count"></div>
  <p id="reconnect-status">waiting for device</p>
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
  const onSuccess = () => {
    fill.style.width = '100%';
    st.className = 'status ok';
    st.textContent = 'done';
    startReconnect();
  };
  xhr.onload = () => {
    if (xhr.status === 200) onSuccess();
    else { st.className='status error'; st.textContent='upload failed'; btn.disabled=false; }
  };
  xhr.onerror = () => {
    if (done) onSuccess();
    else { st.className='status error'; st.textContent='connection lost'; btn.disabled=false; }
  };
  const form = new FormData();
  form.append('file', file);
  xhr.send(form);
}
function startReconnect() {
  document.getElementById('main').style.display = 'none';
  document.getElementById('reconnect').style.display = 'block';
  let elapsed = 0;
  const countEl = document.getElementById('count');
  const statusEl = document.getElementById('reconnect-status');
  const tick = setInterval(() => { elapsed++; countEl.textContent = elapsed+'s'; }, 1000);
  const poll = () => {
    fetch('/ota', { cache: 'no-store' })
      .then(r => { if (r.ok) { clearInterval(tick); statusEl.textContent = 'back online'; window.location.replace('/ota'); } else setTimeout(poll, 2000); })
      .catch(() => setTimeout(poll, 2000));
  };
  setTimeout(poll, 4000);
}
</script>
</body>
</html>
)html";

// ── Upload handler ────────────────────────────────────────────────────────────

static void handleUpload(AsyncWebServerRequest *req, String filename,
                         size_t index, uint8_t *data, size_t len, bool final,
                         int type) {
    if (!hasSession(req)) { Update.abort(); return; }
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

// ── Public API ────────────────────────────────────────────────────────────────

namespace EspOta {

void init(AsyncWebServer &server, const char *password) {
    if (password) _password = String(password);

    // Generate per-boot session token
    char buf[17];
    snprintf(buf, sizeof(buf), "%08x%08x", (unsigned)esp_random(), (unsigned)esp_random());
    _sessionToken = String(buf);

    // Restore boot mode from NVS
    loadBootMode();

    // GET /ota — login form or boot mode UI
    server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!hasSession(req)) {
            req->send(200, "text/html", LOGIN_HTML);
            return;
        }
        if (!_bootMode) saveBootMode(true);
        req->send(200, "text/html", OTA_HTML);
    });

    // POST /ota/login — validate password, set session cookie, enter boot mode
    server.on("/ota/login", HTTP_POST, [](AsyncWebServerRequest *req) {
        String input = req->hasParam("p", true) ? req->getParam("p", true)->value() : "";
        if (!_password.isEmpty() && input != _password) {
            req->send(200, "text/html", LOGIN_HTML);
            return;
        }
        saveBootMode(true);
        AsyncWebServerResponse *resp = req->beginResponse(302, "text/plain", "");
        resp->addHeader("Location", "/ota");
        applySessionCookie(resp);
        req->send(resp);
    });

    // GET /ota/exit — confirm firmware, clear boot mode, redirect home
    server.on("/ota/exit", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!hasSession(req)) {
            req->redirect("/ota");
            return;
        }
        saveBootMode(false);
        esp_ota_mark_app_valid_cancel_rollback();
        AsyncWebServerResponse *resp = req->beginResponse(302, "text/plain", "");
        resp->addHeader("Location", "/");
        clearSessionCookie(resp);
        req->send(resp);
    });

    // POST /update/firmware
    server.on("/update/firmware", HTTP_POST,
        [](AsyncWebServerRequest *req) {
            if (!hasSession(req)) { req->send(403, "application/json", "{\"error\":\"unauthorized\"}"); return; }
            bool ok = !Update.hasError();
            req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"flash failed\"}");
        },
        [](AsyncWebServerRequest *req, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {
            handleUpload(req, filename, index, data, len, final, U_FLASH);
        }
    );

    // POST /update/filesystem
    server.on("/update/filesystem", HTTP_POST,
        [](AsyncWebServerRequest *req) {
            if (!hasSession(req)) { req->send(403, "application/json", "{\"error\":\"unauthorized\"}"); return; }
            bool ok = !Update.hasError();
            req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"flash failed\"}");
        },
        [](AsyncWebServerRequest *req, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {
            handleUpload(req, filename, index, data, len, final, U_SPIFFS);
        }
    );
}

bool rebootPending() { return _rebootPending; }
bool isBootMode()    { return _bootMode; }

void confirm() {
    esp_ota_mark_app_valid_cancel_rollback();
}

}
