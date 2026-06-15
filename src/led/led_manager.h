#pragma once
#include <FastLED.h>

#define LED_PIN     38
#define LED_STRIPS  2
#define LEDs_PER_STRIP 125
#define NUM_LEDS    (LED_STRIPS * LEDs_PER_STRIP)
#define LED_TYPE    WS2812B
#define COLOR_ORDER BRG

class LedManager {
public:
    void begin();
    void update();
    void setLedRaw(uint16_t index, CRGB color);  // Set without calling show()
    void show();  

    // Basic controls
    void setAll(CRGB color);
    void setLed(uint16_t index, CRGB color);
    void clear();
    void setBrightness(uint8_t brightness);

    // Test patterns
    void showRainbow(uint8_t deltaHue = 7);
    void showSolid(CRGB color);
    void showChase(CRGB color, uint16_t speed = 50);

private:
    CRGB _leds[NUM_LEDS];
    uint8_t _brightness = 128;
    uint32_t _lastUpdate = 0;
    uint8_t _chasePos = 0;
    uint8_t _rainbowHue = 0;
};