#include <Arduino.h>
#include "led/led_manager.h"
#include "counter/counter_manager.h"
#include "relay/relay_manager.h"
#include "network/network_manager.h"
#include "webserver/web_manager.h"
#include "websocket/ws_manager.h"
#include "websocket/coil_map.h"

LedManager leds;
CounterManager counters;
RelayManager relays;
EthManager network;
RoleManager     roleManager;
WebManager      web(network, roleManager);       // Pass network ref so web can read/write prefs
WsManager       ws;

#define DEBUG_SERIAL false           // Set false to silence all Serial output
bool _debugSerial = DEBUG_SERIAL;

// ─── Coil callback ────────────────────────────────────────────────────────────
// Fired by WsManager whenever a plcIoChange arrives

void onCoilUpdate(const bool* coils, uint8_t count) {
    const RoleConfig& role = roleManager.getConfig();

    // Safety check — guard against shorter-than-expected coil arrays
    auto coilActive = [&](uint8_t c) -> bool {
        return c < count && coils[c];
    };

    // Match reset → clear all counters
    if (coilActive(COIL_MATCH_RESET)) {
        Serial.println("[MAIN] Match reset → clearing counters");
        counters.resetAll();
    }

   // Role-specific relay and LED logic
    if (coilActive(role.coilMotor)) {
        relays.setState(0, true);
        relays.setState(1, true);
    } else {
        relays.setState(0, false);
        relays.setState(1, false);
    }

    if (coilActive(role.coilLight)) {
        // Color per role
        if (role.role == ROLE_RED_HUB) {
            leds.showSolid(CRGB::Red);
        } else if (role.role == ROLE_BLUE_HUB) {
            leds.showSolid(CRGB::Blue);
        }
    } else if (coilActive(COIL_FIELD_RESET_LIGHT)) { // Only turn off if not also active for motor
        leds.showSolid(CRGB::Green);
    } else {
        leds.showSolid(CRGB::Black);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("[BOOT] Starting...");

    leds.begin();
    //leds.showSolid(CRGB::Red);   // Startup indicator

    roleManager.begin();            // Load role before anything that needs it

    counters.begin();
    counters.addChannel(0, GPIO_NUM_15);    // Counter 1
    counters.addChannel(1, GPIO_NUM_1);     // Counter 2
    counters.addChannel(2, GPIO_NUM_2);     // Counter 3
    counters.addChannel(3, GPIO_NUM_3);     // Counter 4
    counters.startTask();                   //must be called after all channels added

    relays.begin();
    relays.addChannel(0, GPIO_NUM_33); // Horizontal hub motor relay
    relays.addChannel(1, GPIO_NUM_34); // Vertical hub motor relay

    network.begin();
    web.begin();                    // Start webserver after network is ready

     // Start WebSocket — prefs loaded inside begin()
    ws.onCoilUpdate(onCoilUpdate);
    ws.begin("192.168.10.248", 0);                // Empty = use stored prefs
}

void loop()
{
    network.update();
    ws.update();                    // Must be called every loop
    web.update();               // Handles pending reboot

    // Send input states every 500ms
    static uint32_t lastInputSend = 0;
    if (millis() - lastInputSend >= 500) {
        lastInputSend = millis();
        // TODO: populate from actual GPIO reads once role pin mapping is added
        // must add setInputs endpoint in Arena server to use this data
        // bool inputs[INPUT_COUNT] = {};
        // ws.sendInputs(inputs, INPUT_COUNT);
    }

    // Push counter values to arena server every 500ms
    static uint32_t lastSend = 0;
    if (millis() - lastSend >= 500) {
        lastSend = millis();
        ws.sendCounters(
            counters.getCount(0),
            counters.getCount(1),
            counters.getCount(2),
            counters.getCount(3),
            roleManager.getConfig()
        );
    }
  

    // Print count every 2 seconds
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        Serial.printf("[STATUS] Role:%s  Ch0:%lld Ch1:%lld Ch2:%lld Ch3:%lld | Relay:%s | WS:%s\n",
                      roleManager.getRoleName().c_str(),
                      counters.getCount(0), counters.getCount(1),
                      counters.getCount(2), counters.getCount(3),
                      relays.getState(0) ? "ON"  : "OFF",
                      ws.isConnected()   ? "UP"  : "DOWN");
    }
}