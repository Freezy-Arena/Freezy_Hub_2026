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
WebManager      web(network);       // Pass network ref so web can read/write prefs
WsManager       ws;

#define DEBUG_SERIAL false           // Set false to silence all Serial output
bool _debugSerial = DEBUG_SERIAL;

// ─── Coil callback ────────────────────────────────────────────────────────────
// Fired by WsManager whenever a plcIoChange arrives

void onCoilUpdate(const bool* coils, uint8_t count) {

    // Safety check — guard against shorter-than-expected coil arrays
    auto coilActive = [&](PlcCoil c) -> bool {
        return c < count && coils[c];
    };

    // Match reset → clear all counters
    if (coilActive(COIL_MATCH_RESET)) {
        Serial.println("[MAIN] Match reset → clearing counters");
        counters.resetAll();
    }

   
    // Relay logic
    // hub motors
    relays.setState(0, coilActive(COIL_RED_HUB_MOTOR));
    relays.setState(1, coilActive(COIL_RED_HUB_MOTOR));
    // Lights
    if (coilActive(COIL_RED_HUB_LIGHT)){
        leds.showSolid(CRGB::Red);
    }else{
        leds.showSolid(CRGB::Black);
    }
    // Add more logic here to react to other coils as needed}
}

void _onCoilUpdate(const bool* coils, uint8_t count) {
    // Example: Coil[1] TRUE → reset all counters (mirrors Python behaviour)
    if (count > 1 && coils[1]) {
        Serial.println("[MAIN] Coil[1] ON → resetting counters");
        counters.resetAll();
    }

    // Example: Coil[2] drives relay 0
    if (count > 2) {
        relays.setState(0, coils[2]);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("[BOOT] Starting...");

    leds.begin();
    //leds.showSolid(CRGB::Red);   // Startup indicator

    counters.begin();
    counters.addChannel(0, GPIO_NUM_15); // Counter 1
    counters.addChannel(1, GPIO_NUM_1);  // Counter 2
    counters.addChannel(2, GPIO_NUM_2);  // Counter 3
    counters.addChannel(3, GPIO_NUM_3);  // Counter 4

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
    // leds.showChase(CRGB::Green);   // Swap in any pattern to test
    // leds.showRainbow();
    /* leds.showSolid(CRGB::Blue);
    delay(1000);
    leds.showSolid(CRGB::Green);
    delay(1000);
    leds.showSolid(CRGB::Red);
    delay(1000); */

    counters.update();
    network.update();
    ws.update();                    // Must be called every loop

    // Push counter values to arena server every 500ms
    static uint32_t lastSend = 0;
    if (millis() - lastSend >= 500) {
        lastSend = millis();
        ws.sendCounters(
            counters.getCount(0),
            counters.getCount(1),
            counters.getCount(2),
            counters.getCount(3)
        );
    }
  

    // Print count every 2 seconds
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 2000)
    {
        lastPrint = millis();
        Serial.printf("[COUNTER] Ch0: %lld\n", counters.getCount(0));
        Serial.printf("[COUNTER] Ch1: %lld\n", counters.getCount(1));
        Serial.printf("[COUNTER] Ch2: %lld\n", counters.getCount(2));
        Serial.printf("[COUNTER] Ch3: %lld\n", counters.getCount(3));
    }
}