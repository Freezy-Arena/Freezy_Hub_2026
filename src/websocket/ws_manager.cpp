#include "ws_manager.h"
#include <Preferences.h>

// ─── Preferences ─────────────────────────────────────────────────────────────

void WsManager::loadPreferences() {
    _prefs.begin(WS_PREFS_NS, true);
    arenaHost = _prefs.getString("arenaHost", "10.0.100.5");
    arenaPort = _prefs.getUShort("arenaPort", 8080);
    _prefs.end();
    //Serial.printf("[WS] Prefs loaded — %s:%d\n", arenaHost.c_str(), arenaPort);
    WS_LOG("[WS] Prefs loaded — %s:%d\n", arenaHost.c_str(), arenaPort);
}

void WsManager::savePreferences() {
    _prefs.begin(WS_PREFS_NS, false);
    _prefs.putString("arenaHost", arenaHost);
    _prefs.putUShort("arenaPort", arenaPort);
    _prefs.end();
    Serial.println("[WS] Prefs saved");
}

// ─── Begin ────────────────────────────────────────────────────────────────────

void WsManager::begin(const String& host, uint16_t port, const String& path) {
    loadPreferences();

    // Allow override from caller (used before prefs are populated)
    if (host.length()) arenaHost = host;
    if (port)          arenaPort = port;


    _ws.begin(arenaHost.c_str(), arenaPort, path.c_str());
    String origin = "Origin: http://" + arenaHost + ":" + String(arenaPort);
    _ws.setExtraHeaders(origin.c_str());
    _ws.onEvent([this](WStype_t t, uint8_t* p, size_t l) {
        _onEvent(t, p, l);
    });
    _ws.setReconnectInterval(WS_RECONNECT_DELAY_MS);

    Serial.printf("[WS] Connecting to ws://%s:%d%s\n", arenaHost.c_str(), arenaPort, path.c_str());
}

// ─── Update (call from loop) ──────────────────────────────────────────────────

void WsManager::update() {
    _ws.loop();
}

// ─── Connection state ─────────────────────────────────────────────────────────

bool WsManager::isConnected() {
    return _connected;
}

// ─── Coil callback registration ──────────────────────────────────────────────

void WsManager::onCoilUpdate(CoilCallback cb) {
    _coilCb = cb;
}

// ─── Send helpers ─────────────────────────────────────────────────────────────

void WsManager::_sendJson(const String& type, JsonDocument& doc) {
    if (!_connected) {
        Serial.println("[WS] Not connected — message dropped");
        return;
    }
    String out;
    serializeJson(doc, out);
    _ws.sendTXT(out);
    //Serial.printf("[WS] → %s\n", out.c_str());
    WS_LOG("[WS] → %s\n", out.c_str());
}

void WsManager::sendInputs(const bool* states, uint8_t count) {
    if (!_connected) return;

    JsonDocument doc;
    doc["type"] = "setInput";     
    JsonArray arr = doc["data"].to<JsonArray>();

    for (uint8_t i = 0; i < count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["channel"]  = i;
        o["state"] = states[i];
    }

    _sendJson("setInput", doc);
}
void WsManager::sendInput(const bool state, uint8_t channel) { // Single channel version for testing
    if (!_connected) return;

    JsonDocument doc;
    doc["type"] = "setInput";   
    JsonArray arr = doc["data"].to<JsonArray>();

    JsonObject o = arr.add<JsonObject>();
    o["channel"]  = channel;
    o["state"] = state;

    _sendJson("setInput", doc);
}

void WsManager::sendCounters(int64_t ch0, int64_t ch1,
                              int64_t ch2, int64_t ch3,
                              const RoleConfig& role) {
    if (!_connected) return;

    int64_t total = ch0 + ch1 + ch2 + ch3;

    JsonDocument doc;
    doc["type"] = "setRegisters";
    JsonArray arr = doc["data"].to<JsonArray>();

    auto addReg = [&](uint8_t reg, int64_t val) {
        JsonObject o = arr.add<JsonObject>();
        o["register"] = reg;
        o["cValue"]   = (long)val;
    };

    addReg(role.regCh0,   ch0);
    addReg(role.regCh1,   ch1);
    addReg(role.regCh2,   ch2);
    addReg(role.regCh3,   ch3);
    addReg(role.regTotal, total);

    _sendJson("setRegisters", doc);
}

// ─── Inbound message handling ─────────────────────────────────────────────────

void WsManager::_handleCoilChange(JsonObject data) {
    // Payload shape: {"Coils":[false,true,false,...]}
    JsonArray coilArr = data["Coils"].as<JsonArray>();
    if (coilArr.isNull()) {
        Serial.println("[WS] plcIoChange: no Coils array found");
        return;
    }

    uint8_t count = coilArr.size();
    bool coils[count];
    for (uint8_t i = 0; i < count; i++) {
        coils[i] = coilArr[i].as<bool>();
    }

    // Print all coil states
    Serial.printf("[WS] Coils(%d):", count);
    for (uint8_t i = 0; i < count; i++) {
        Serial.printf("  [%d]=%s", i, coils[i] ? "ON" : "OFF");
    }
    Serial.println();

    // Fire registered callback
    if (_coilCb) _coilCb(coils, count);
}

void WsManager::_handleMessage(const String& raw) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw);
    if (err) {
        WS_LOG("[WS] JSON parse error: %s\n", err.c_str());
        return;
    }

    String type = doc["type"].as<String>();
    JsonObject data = doc["data"].as<JsonObject>();

    // === Print the full raw JSON (recommended) ===
    WS_LOG("[WS] ← Received JSON:");
    if (_debugSerial){
        serializeJsonPretty(doc, Serial);  // Pretty print
    }
    WS_LOG(" ");  // Extra newline for readability

    if (type == "plcIoChange") {
        Serial.printf("[WS] ← plcIoChange received\n");
        _handleCoilChange(data);
    } else if (type == "arenaStatus") {
        Serial.printf("[WS] ← arenaStatus received\n");
    } else if (type == "plcRegisterSetSuccess") {
        WS_LOG("[WS] ← Unhandled type: %s\n", type.c_str());
        WS_LOG("[WS] ← Register set ACK");
    } else if (type == "setLedMode") {
        Serial.printf("[WS] ← setLedMode: %s\n", type.c_str());
        serializeJsonPretty(doc, Serial);  // Pretty print
        _handleLedMode(data);
    } else if (type == "ping") {
        Serial.printf("[WS] ← Ping received");
    } else if (type == "error") {
        Serial.printf("[WS] ← Server error: %s\n",
                      doc["data"].as<String>().c_str());
    } else {
        WS_LOG("[WS] ← Unhandled type: %s\n", type.c_str());
    }
}

// ─── WebSocket event handler ──────────────────────────────────────────────────

void WsManager::_onEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            _connected = true;
            Serial.printf("[WS] Connected to %s:%d\n",
                          arenaHost.c_str(), arenaPort);
            break;

        case WStype_DISCONNECTED:
            _connected = false;
            Serial.println("[WS] Disconnected — will retry");
            break;

        case WStype_TEXT:
            _handleMessage(String((char*)payload));
            break;

        case WStype_ERROR:
            Serial.println("[WS] Socket error");
            break;

        default:
            break;
    }
    
}

void WsManager::onLedMode(LedModeCallback cb) {
    _ledModeCb = cb;
}

void WsManager::_handleLedMode(JsonObject data) {
    LedMode redMode  = (LedMode)data["RedMode"].as<uint8_t>();
    LedMode blueMode = (LedMode)data["BlueMode"].as<uint8_t>();
    WS_LOG("[WS] ← setLedMode red=%d blue=%d\n", redMode, blueMode);
    if (_ledModeCb) _ledModeCb(redMode, blueMode);
}