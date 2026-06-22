#include <Arduino.h>
#include "led/led_manager.h"
#include "counter/counter_manager.h"
#include "relay/relay_manager.h"
#include "network/network_manager.h"
#include "webserver/web_manager.h"
#include "websocket/ws_manager.h"
#include "websocket/coil_map.h"
#include "websocket/input_map.h"
#include "dmx_led/dmx_led_manager.h"

LedManager leds;
CounterManager counters;
RelayManager relays;
EthManager network;
RoleManager     roleManager;
WsManager       ws;
WebManager      web(network, roleManager, leds, ws);       // Pass managers so web can read/write prefs
DmxLedManager   dmxLed(leds, roleManager);

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

    // LED only in coil mode
    if (network.ledControlMode != LED_CONTROL_COIL) return;

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

void onLedStatusUpdate(JsonArray redPixels, JsonArray bluePixels) {
    if (network.ledControlMode != LED_CONTROL_WEBSOCKET) return;

    const bool isRedHub = roleManager.getRole() == ROLE_RED_HUB;
    JsonArray pixels = isRedHub ? redPixels : bluePixels;
    uint16_t ledCount = leds.getLedCount();
    uint16_t pixelCount = pixels.size();
    uint16_t copyCount = min(ledCount, pixelCount);

    for (uint16_t i = 0; i < copyCount; i++) {
        JsonObject pixel = pixels[i].as<JsonObject>();
        uint8_t red = (uint8_t)constrain(pixel["R"].as<int>(), 0, 255);
        uint8_t green = (uint8_t)constrain(pixel["G"].as<int>(), 0, 255);
        uint8_t blue = (uint8_t)constrain(pixel["B"].as<int>(), 0, 255);
        leds.setLedRaw(i, CRGB(red, green, blue));
    }

    for (uint16_t i = copyCount; i < ledCount; i++) {
        leds.setLedRaw(i, CRGB::Black);
    }

    leds.show();
    Serial.printf("[LED] Applied %s ledStatus frame: %u pixels (%u LEDs configured)\n",
                  isRedHub ? "Red" : "Blue",
                  (unsigned)pixelCount, (unsigned)ledCount);
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("[BOOT] Starting...");

    roleManager.begin();            // Load role before anything that needs it

    const RoleConfig& role = roleManager.getConfig();

    counters.begin();
    for (uint8_t i = 0; i < 4; i++) {
        counters.addChannel(i, role.counterPin[i]);
    }
    counters.startTask();                   //must be called after all channels added

    relays.begin();
    relays.addChannel(0, role.relayMotor); // Horizontal hub motor relay
    relays.addChannel(1, role.relayLight); // Vertical hub motor relay

    leds.begin();

    network.begin();
    web.begin();                    // Start webserver after network is ready

     // Start WebSocket — prefs loaded inside begin()
    ws.onCoilUpdate(onCoilUpdate);
    ws.onLedStatus(onLedStatusUpdate);
    ws.begin("", 0);                // Empty = use stored prefs

    dmxLed.begin();
}

void loop()
{
    network.update();
    ws.setLedStatusEnabled(network.ledControlMode == LED_CONTROL_WEBSOCKET);
    ws.update();                    // Must be called every loop
    web.update();               // Handles pending reboot

    LedControlMode ledMode = network.ledControlMode;

    // DMX direct — handled by dmxLed.update()
    if (ledMode == LED_CONTROL_DMX) {
        dmxLed.update();
    }
    // WebSocket LED frames are applied when ledStatus messages arrive.

    // Coil mode — handled in onCoilUpdate
    // Only pass coil LED updates through if in coil mode

    // Send input states every 500ms
    static uint32_t lastInputSend = 0;
    if (millis() - lastInputSend >= 500) {
        lastInputSend = millis();
        const RoleConfig& role = roleManager.getConfig();
        bool state = digitalRead(role.counterPin[0]);  // Same pin as counter channel 0
        ws.sendInput(state, role.plcInputSensor[0]); // Map to role-specific input index
        // TODO: loop trhough all role-specific inputs and send as batch
        // create a list of couterpins to interate through

        
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
