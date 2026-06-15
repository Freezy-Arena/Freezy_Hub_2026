#include "led_manager.h"

void LedManager::begin() {
    loadPreferences();
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(_leds, LED_MAX_LEDS)
           .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(_brightness);
    clear();
    FastLED.show();
    Serial.printf("[LED] Initialized with %u LEDs\n", _ledCount);
}

void LedManager::update() {
    // Hook for animations that need periodic updates
    // Called from main loop
}

void LedManager::setLedRaw(uint16_t index, CRGB color) {
    if (index < _ledCount) {
        _leds[index] = color;
    }
}

void LedManager::show() {
    FastLED.show();
}

void LedManager::setAll(CRGB color) {
    fill_solid(_leds, _ledCount, color);
    if (_ledCount < LED_MAX_LEDS) {
        fill_solid(_leds + _ledCount, LED_MAX_LEDS - _ledCount, CRGB::Black);
    }
    FastLED.show();
}

void LedManager::setLed(uint16_t index, CRGB color) {
    if (index < _ledCount) {
        _leds[index] = color;
        FastLED.show();
    }
}

void LedManager::clear() {
    fill_solid(_leds, LED_MAX_LEDS, CRGB::Black);
    FastLED.show();
}

void LedManager::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    FastLED.setBrightness(_brightness);
    FastLED.show();
}

void LedManager::showRainbow(uint8_t deltaHue) {
    fill_rainbow(_leds, _ledCount, _rainbowHue, deltaHue);
    if (_ledCount < LED_MAX_LEDS) {
        fill_solid(_leds + _ledCount, LED_MAX_LEDS - _ledCount, CRGB::Black);
    }
    _rainbowHue++;
    FastLED.show();
}

void LedManager::showSolid(CRGB color) {
    setAll(color);
}

void LedManager::showChase(CRGB color, uint16_t speed) {
    if (millis() - _lastUpdate < speed) return;
    _lastUpdate = millis();

    if (_ledCount == 0) return;

    clear();
    _leds[_chasePos % _ledCount] = color;
    FastLED.show();
    _chasePos++;
}

uint16_t LedManager::getLedCount() const {
    return _ledCount;
}

uint16_t LedManager::getMaxLedCount() const {
    return LED_MAX_LEDS;
}

void LedManager::setLedCount(uint16_t count) {
    _ledCount = constrain(count, (uint16_t)1, (uint16_t)LED_MAX_LEDS);
}

void LedManager::loadPreferences() {
    _prefs.begin(LED_PREFS_NS, true);
    setLedCount(_prefs.getUShort("count", LED_DEFAULT_COUNT));
    _prefs.end();
}

void LedManager::savePreferences() {
    _prefs.begin(LED_PREFS_NS, false);
    _prefs.putUShort("count", _ledCount);
    _prefs.end();
    Serial.printf("[LED] Prefs saved - count: %u\n", _ledCount);
}
