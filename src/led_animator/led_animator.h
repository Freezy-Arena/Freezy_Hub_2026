#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "../led/led_manager.h"
#include "../role_config.h"

// Mirrors led/mode.go
enum LedMode : uint8_t {
    LED_MODE_OFF            = 0,
    LED_MODE_RED            = 1,
    LED_MODE_BLUE           = 2,
    LED_MODE_GREEN          = 3,
    LED_MODE_PURPLE         = 4,
    LED_MODE_WHITE          = 5,
    LED_MODE_RED_PULSE      = 6,
    LED_MODE_BLUE_PULSE     = 7,
    LED_MODE_RED_STARTUP    = 8,
    LED_MODE_BLUE_STARTUP   = 9,
    LED_MODE_RED_ADVANTAGE  = 10,
    LED_MODE_BLUE_ADVANTAGE = 11,
    LED_MODE_RAINBOW        = 12,
    LED_MODE_SIDE_1_TEST    = 13,
    LED_MODE_SIDE_2_TEST    = 14,
    LED_MODE_SIDE_3_TEST    = 15,
    LED_MODE_SIDE_4_TEST    = 16
};

// Mirrors strip.go constants
#define ANIM_PULSE_HALF_PERIOD      70
#define ANIM_STARTUP_CYCLES         100
#define ANIM_ADVANTAGE_STEP_CYCLE   4
#define ANIM_UPDATE_MS              16      // ~60fps, matches arena loop

class LedAnimator {
public:
    LedAnimator(LedManager& leds, RoleManager& role);

    void begin();
    void update();                          // Call from loop()
    void setMode(LedMode redMode, LedMode blueMode);

    LedMode activeMode();                   // Current mode for this role

private:
    LedManager&     _leds;
    RoleManager&    _role;

    LedMode         _redMode    = LED_MODE_OFF;
    LedMode         _blueMode   = LED_MODE_OFF;
    int             _counter    = 0;
    uint32_t        _lastUpdate = 0;

    void _renderMode(LedMode mode);
    void _renderSolid(CRGB color);
    void _renderPulse(CRGB color);
    void _renderStartup(CRGB color);
    void _renderAdvantage(CRGB color);
    void _renderRainbow();
    void _renderSideTest(uint8_t side);

    void _fillSide(int side, CRGB color, int counter, int direction);
    void _fillFixture(int startLed, CRGB color, float percentage, int direction, int count);
    void _sweepFixture(int startLed, int counter, int direction, int count);

    CRGB _rainbowColor(uint8_t position);
    CRGB _colorForMode(LedMode mode);
};
