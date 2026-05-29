#include <Arduino.h>
#include "led/led_manager.h"
#include "counter/counter_manager.h"
#include "relay/relay_manager.h"

LedManager leds;
CounterManager  counters;
RelayManager    relays;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[BOOT] Starting...");

    //leds.begin();
    //leds.showSolid(CRGB::Red);   // Startup indicator

    counters.begin();
    counters.addChannel(0, GPIO_NUM_15);  // Counter 1
    counters.addChannel(1, GPIO_NUM_1);   // Counter 2
    counters.addChannel(2, GPIO_NUM_2);   // Counter 3
    counters.addChannel(3, GPIO_NUM_3);   // Counter 4

    relays.begin();
    relays.addChannel(0, GPIO_NUM_33);
}

void loop() {
    //leds.showChase(CRGB::Green);   // Swap in any pattern to test
    //leds.showRainbow();
    /* leds.showSolid(CRGB::Blue);
    delay(1000);
    leds.showSolid(CRGB::Green);
    delay(1000);
    leds.showSolid(CRGB::Red);
    delay(1000); */

    counters.update();

    // Relay logic — on when counter 0 exceeds 10
    relays.setState(0, counters.getCount(0) > 10);

    // Print count every 2 seconds
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        Serial.printf("[COUNTER] Ch0: %lld\n", counters.getCount(0));
        Serial.printf("[COUNTER] Ch1: %lld\n", counters.getCount(1));
        Serial.printf("[COUNTER] Ch2: %lld\n", counters.getCount(2));
        Serial.printf("[COUNTER] Ch3: %lld\n", counters.getCount(3));
    }
}