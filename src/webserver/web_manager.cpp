#include "web_manager.h"

WebManager::WebManager(EthManager& eth, RoleManager& role)
    : _server(80), _eth(eth), _role(role) {}

void WebManager::begin() {
    _setupRoutes();
    _server.begin();
    Serial.printf("[WEB] Server started at http://%s\n",
                  _eth.localIPString().c_str());
}

// ─── Page builder ────────────────────────────────────────────────────────────
String WebManager::_buildPage(const String& message) {
    String ip     = _eth.staticIP;
    String gw     = _eth.staticGW;
    bool  isDHCP  = _eth.useDHCP;
    uint8_t ledMode = (uint8_t)_eth.ledControlMode;

    String html = R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Device Configuration</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0f1117;
      color: #e2e8f0;
      min-height: 100vh;
      display: flex;
      align-items: flex-start;
      justify-content: center;
      padding: 40px 16px;
    }
    .card {
      background: #1a1d27;
      border: 1px solid #2d3148;
      border-radius: 12px;
      padding: 36px;
      width: 100%;
      max-width: 480px;
    }
    .header {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 28px;
    }
    .dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      background: #22c55e;
      box-shadow: 0 0 8px #22c55e;
      flex-shrink: 0;
    }
    h1 { font-size: 1.2rem; font-weight: 600; letter-spacing: 0.02em; color: #f1f5f9; }
    .status-block {
      background: #12151f;
      border: 1px solid #2d3148;
      border-radius: 8px;
      padding: 14px 18px;
      margin-bottom: 28px;
      font-size: 0.85rem;
    }
    .status-row { display: flex; justify-content: space-between; padding: 4px 0; color: #94a3b8; }
    .status-row span:last-child { color: #e2e8f0; font-family: monospace; font-size: 0.9rem; }
    .message { padding: 12px 16px; border-radius: 8px; margin-bottom: 20px; font-size: 0.875rem; font-weight: 500; }
    .message.error  { background: #2d1515; border: 1px solid #7f1d1d; color: #fca5a5; }
    .message.success { background: #14281e; border: 1px solid #14532d; color: #86efac; }
    h2 { font-size: 0.8rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.08em; color: #64748b; margin-bottom: 18px; }
    .field { margin-bottom: 18px; }
    label { display: block; font-size: 0.85rem; color: #94a3b8; margin-bottom: 6px; }
    input[type="text"], select {
      width: 100%;
      padding: 10px 14px;
      background: #12151f;
      border: 1px solid #2d3148;
      border-radius: 8px;
      color: #e2e8f0;
      font-size: 0.95rem;
      font-family: monospace;
      transition: border-color 0.2s;
      outline: none;
    }
    input[type="text"]:focus, select:focus { border-color: #6366f1; }
    input[type="text"]:disabled { opacity: 0.4; cursor: not-allowed; }
    .toggle-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      background: #12151f;
      border: 1px solid #2d3148;
      border-radius: 8px;
      padding: 12px 16px;
      margin-bottom: 18px;
    }
    .toggle-label { font-size: 0.9rem; color: #e2e8f0; }
    .toggle-label small { display: block; font-size: 0.78rem; color: #64748b; margin-top: 2px; }
    .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; inset: 0; background: #2d3148; border-radius: 24px; cursor: pointer; transition: background 0.2s; }
    .slider::before { content: ''; position: absolute; width: 18px; height: 18px; left: 3px; bottom: 3px; background: #94a3b8; border-radius: 50%; transition: transform 0.2s, background 0.2s; }
    input:checked + .slider { background: #6366f1; }
    input:checked + .slider::before { transform: translateX(20px); background: #fff; }
    .divider { border: none; border-top: 1px solid #2d3148; margin: 24px 0; }
    button[type="submit"] {
      width: 100%; padding: 12px;
      background: #6366f1; color: #fff;
      border: none; border-radius: 8px;
      font-size: 0.95rem; font-weight: 600;
      cursor: pointer;
      transition: background 0.2s, opacity 0.2s;
      letter-spacing: 0.02em;
    }
    button[type="submit"]:hover { background: #4f46e5; }
    button[type="submit"]:active { opacity: 0.8; }
  </style>
  <script>
    function toggleStatic(checked) {
      var fields = document.querySelectorAll('.static-field');
      fields.forEach(function(f) {
        f.querySelector('input').disabled = checked;
        f.style.opacity = checked ? '0.4' : '1';
      });
    }
    window.onload = function() {
      var cb = document.getElementById('dhcpToggle');
      toggleStatic(cb.checked);
      cb.addEventListener('change', function() { toggleStatic(this.checked); });
    };
  </script>
</head>
<body>
  <div class="card">
    <div class="header">
      <div class="dot"></div>
      <h1>Device Configuration</h1>
    </div>)";

    // Message banner
    if (message.length()) {
        bool isError = message.startsWith("ERROR");
        html += "<div class='message " + String(isError ? "error" : "success") + "'>"
                + message + "</div>";
    }

    // Status block
    html += R"(
    <div class="status-block">
      <div class="status-row"><span>IP Address</span><span>)"
    + _eth.localIPString() + R"(</span></div>
      <div class="status-row"><span>Mode</span><span>)"
    + String(isDHCP ? "DHCP" : "Static") + R"(</span></div>
    </div>

    <h2>Network Settings</h2>
    <form method="POST" action="/save">

      <div class="toggle-row">
        <div class="toggle-label">
          Use DHCP
          <small>Automatically assign IP address</small>
        </div>
        <label class="switch">
          <input type="checkbox" id="dhcpToggle" name="useDHCP" value="1" )"
    + String(isDHCP ? "checked" : "") + R"(>
          <span class="slider"></span>
        </label>
      </div>

      <div class="field static-field">
        <label>Static IP Address</label>
        <input type="text" name="staticIP" value=")" + ip + R"(" placeholder="192.168.1.200">
      </div>

      <div class="field static-field">
        <label>Gateway</label>
        <input type="text" name="staticGW" value=")" + gw + R"(" placeholder="192.168.1.1">
      </div>

      <hr class="divider">
      <h2>Device Role</h2>
      <div class="field">
        <label>Role</label>
        <select name="role">
          <option value="redHub" )"
    + String(_role.getRoleName() == "redHub"  ? "selected" : "") + R"(>Red Hub</option>
          <option value="blueHub" )"
    + String(_role.getRoleName() == "blueHub" ? "selected" : "") + R"(>Blue Hub</option>
        </select>
      </div>

      <hr class="divider">
      <h2>LED Control</h2>
      <div class="field">
        <label>Control Mode</label>
        <select name="ledControl">
          <option value="0" )" + String(ledMode == 0 ? "selected" : "") + R"(>Coil</option>
          <option value="1" )" + String(ledMode == 1 ? "selected" : "") + R"(>DMX Direct</option>
          <option value="2" )" + String(ledMode == 2 ? "selected" : "") + R"(>DMX WebSocket</option>
        </select>
      </div>

      <hr class="divider">
      <button type="submit">Save &amp; Reboot</button>
    </form>
  </div>
</body>
</html>)";

    return html;
}

// ─── Routes ──────────────────────────────────────────────────────────────────

void WebManager::_setupRoutes() {

    // GET / — config page
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "text/html", _buildPage());
    });

    // POST /save — save and reboot
    _server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* req) {

        // DHCP checkbox — only present in POST if checked
        _eth.useDHCP = req->hasParam("useDHCP", true);

        if (!_eth.useDHCP) {
            if (req->hasParam("staticIP", true))
                _eth.staticIP = req->getParam("staticIP", true)->value();
            if (req->hasParam("staticGW", true))
                _eth.staticGW = req->getParam("staticGW", true)->value();

            // Basic validation
            IPAddress testIP, testGW;
            if (!testIP.fromString(_eth.staticIP) ||
                !testGW.fromString(_eth.staticGW)) {
                req->send(200, "text/html",
                          _buildPage("ERROR: Invalid IP or Gateway — not saved."));
                return;
            }
        }

        // Role
        if (req->hasParam("role", true)) {
            _role.setRoleByName(req->getParam("role", true)->value());
            _role.savePreferences();
        }

        // LED control mode
        if (req->hasParam("ledControl", true)) {
            uint8_t mode = req->getParam("ledControl", true)->value().toInt();
            Serial.printf("[WEB] LED control mode: %d\n", mode);
            _eth.ledControlMode = (LedControlMode)mode;
        }

        _eth.savePreferences();

        String rebootPage = R"(<!DOCTYPE html>
        <html><head>
        <meta charset="UTF-8">
        <meta http-equiv="refresh" content="5;url=/">
        <title>Rebooting</title>
        </head><body style="background:#0f1117;color:#e2e8f0;font-family:sans-serif;
        display:flex;align-items:center;justify-content:center;height:100vh;margin:0;">
        <div style="text-align:center;">
          <h2 style="color:#86efac;">Settings Saved</h2>
          <p style="color:#94a3b8;">Device is rebooting — reconnecting in 5 seconds...</p>
        </div>
        </body></html>)";

        req->send(200, "text/html", rebootPage);
        _rebootPending  = true;
        _rebootAt       = millis() + 3000;
    });

    // 404
    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

}

void WebManager::update() {
    if (_rebootPending && millis() >= _rebootAt) {
        _rebootPending = false;
        Serial.println("[WEB] Rebooting now...");
        ESP.restart();
    }
}