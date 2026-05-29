#pragma once
#include <FastLED.h>

#define LED_PIN     38
#define NUM_LEDS    100
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

class LedManager {
public:
    void begin();
    void update();

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