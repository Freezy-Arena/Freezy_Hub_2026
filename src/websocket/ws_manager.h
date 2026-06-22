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
#define WS_MAX_MESSAGE_SIZE     (15 * 1024)
#define WS_PREFS_NS             "websocket"
extern bool _debugSerial;

#define WS_LOG(fmt, ...) if (_debugSerial) Serial.printf(fmt, ##__VA_ARGS__)
#define WS_PRINTLN(msg)  if (_debugSerial) Serial.println(msg)

// Coil callback — fired whenever coil state changes
// coils: pointer to bool array, count: number of coils
typedef void (*CoilCallback)(const bool* coils, uint8_t count);

// ledStatus mode callback for the server's RedMode and BlueMode fields.
typedef void (*LedStatusCallback)(int redMode, int blueMode);


class WsManager {
public:
    void begin(const String& host, uint16_t port, const String& path = "/setup/field_testing/websocket");
    void update();                          // Call from loop()

    void onLedStatus(LedStatusCallback cb);
    void setLedStatusEnabled(bool enabled);
    
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
    String  arenaHost = "10.0.100.5";
    uint16_t arenaPort = 8080;
    bool sendRegistersEnabled = true;
    bool sendInputsEnabled = true;
    void loadPreferences();
    void savePreferences();

private:
    WebSocketsClient    _ws;
    bool                _connected  = false;
    uint32_t            _lastRetry  = 0;
    bool                _receivingTextFragment = false;
    bool                _ledStatusEnabled = true;
    String              _fragmentBuffer;
    CoilCallback        _coilCb     = nullptr;
    Preferences         _prefs;

    void _onEvent(WStype_t type, uint8_t* payload, size_t length);
    bool _appendTextFragment(const uint8_t* payload, size_t length);
    void _handleMessage(const String& raw);
    void _handleCoilChange(JsonObject data);
    void _sendJson(const String& type, JsonDocument& data);
    LedStatusCallback _ledStatusCb = nullptr;
    void _handleLedStatus(JsonObject data);
};
