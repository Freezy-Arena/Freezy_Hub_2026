#pragma once
#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "../role_config.h"

// Mirrors the Python register map
// Counter channel → PLC register
//   Ch0 → Reg 3    Ch2 → Reg 5
//   Ch1 → Reg 4    Ch3 → Reg 6
// Reg 1 = sum of Reg 3,4,5,6
// Reg 2 = sum of Reg 7,8,9,10  (reserved for future channels)

#define WS_RECONNECT_DELAY_MS   3000
#define WS_PREFS_NS             "websocket"
extern bool _debugSerial;

#define WS_LOG(fmt, ...) if (_debugSerial) Serial.printf(fmt, ##__VA_ARGS__)
#define WS_PRINTLN(msg)  if (_debugSerial) Serial.println(msg)

// Coil callback — fired whenever coil state changes
// coils: pointer to bool array, count: number of coils
typedef void (*CoilCallback)(const bool* coils, uint8_t count);

class WsManager {
public:
    void begin(const String& host, uint16_t port, const String& path = "/api/plc/websocket");
    void update();                          // Call from loop()

    bool isConnected();

    // Send input states 
    void sendInputs(const bool* states, uint8_t count);
    void sendInput(const bool state, uint8_t channel); // Single channel version for testing

    // Send all four counter values as registers in one batch
    void sendCounters(int64_t ch0, int64_t ch1, int64_t ch2, int64_t ch3,
                  const RoleConfig& role);

    // Register a callback for incoming coil updates
    void onCoilUpdate(CoilCallback cb);

    // Preferences — ready for webserver config later
    String  arenaHost = "192.168.10.248";//"10.0.100.5";
    uint16_t arenaPort = 8080;
    void loadPreferences();
    void savePreferences();

private:
    WebSocketsClient    _ws;
    bool                _connected  = false;
    uint32_t            _lastRetry  = 0;
    CoilCallback        _coilCb     = nullptr;
    Preferences         _prefs;

    void _onEvent(WStype_t type, uint8_t* payload, size_t length);
    void _handleMessage(const String& raw);
    void _handleCoilChange(JsonObject data);
    void _sendJson(const String& type, JsonDocument& data);
};