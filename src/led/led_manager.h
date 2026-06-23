#pragma once
#include <FastLED.h>
#include <Preferences.h>

#define LED_PIN     38
#define LED_DEFAULT_COUNT 125
#define LED_MAX_LEDS      300
#define LED_TYPE    WS2812B
//#define COLOR_ORDER BRG
#define COLOR_ORDER GRB
#define LED_PREFS_NS "leds"

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

    // Configuration
    uint16_t getLedCount() const;
    uint16_t getMaxLedCount() const;
    void setLedCount(uint16_t count);
    void loadPreferences();
    void savePreferences();

    // Test patterns
    void showRainbow(uint8_t deltaHue = 7);
    void showSolid(CRGB color);
    void showChase(CRGB color, uint16_t speed = 50);

private:
    CRGB _leds[LED_MAX_LEDS];
    Preferences _prefs;
    uint16_t _ledCount = LED_DEFAULT_COUNT;
    uint8_t _brightness = 128;
    uint32_t _lastUpdate = 0;
    uint16_t _chasePos = 0;
    uint8_t _rainbowHue = 0;
};
