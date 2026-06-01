#include "led_manager.h"

void LedManager::begin() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(_leds, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(_brightness);
    clear();
    FastLED.show();
    Serial.println("[LED] Initialized");
}

void LedManager::update() {
    // Hook for animations that need periodic updates
    // Called from main loop
}

void LedManager::setLedRaw(uint16_t index, CRGB color) {
    if (index < NUM_LEDS) {
        _leds[index] = color;
    }
}

void LedManager::show() {
    FastLED.show();
}

void LedManager::setAll(CRGB color) {
    fill_solid(_leds, NUM_LEDS, color);
    FastLED.show();
}

void LedManager::setLed(uint16_t index, CRGB color) {
    if (index < NUM_LEDS) {
        _leds[index] = color;
        FastLED.show();
    }
}

void LedManager::clear() {
    fill_solid(_leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

void LedManager::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    FastLED.setBrightness(_brightness);
    FastLED.show();
}

void LedManager::showRainbow(uint8_t deltaHue) {
    fill_rainbow(_leds, NUM_LEDS, _rainbowHue, deltaHue);
    _rainbowHue++;
    FastLED.show();
}

void LedManager::showSolid(CRGB color) {
    setAll(color);
}

void LedManager::showChase(CRGB color, uint16_t speed) {
    if (millis() - _lastUpdate < speed) return;
    _lastUpdate = millis();

    clear();
    _leds[_chasePos % NUM_LEDS] = color;
    FastLED.show();
    _chasePos++;
}