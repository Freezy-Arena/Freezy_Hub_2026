#include "led_manager.h"

void LedManager::begin() {
    loadPreferences();
    CLEDController* controller = nullptr;
    switch (_colorOrder) {
        case LED_ORDER_RGB: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, RGB>(_leds, LED_MAX_LEDS); break;
        case LED_ORDER_RBG: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, RBG>(_leds, LED_MAX_LEDS); break;
        case LED_ORDER_GRB: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, GRB>(_leds, LED_MAX_LEDS); break;
        case LED_ORDER_GBR: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, GBR>(_leds, LED_MAX_LEDS); break;
        case LED_ORDER_BRG: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, BRG>(_leds, LED_MAX_LEDS); break;
        case LED_ORDER_BGR: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, BGR>(_leds, LED_MAX_LEDS); break;
        default: controller = &FastLED.addLeds<LED_TYPE, LED_PIN, BRG>(_leds, LED_MAX_LEDS); break;
    }
    controller->setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(_brightness);
    clear();
    FastLED.show();
    Serial.printf("[LED] Initialized with %u LEDs, color order %s\n",
                  _ledCount, getColorOrderName());
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

LedColorOrder LedManager::getColorOrder() const {
    return _colorOrder;
}

const char* LedManager::getColorOrderName() const {
    switch (_colorOrder) {
        case LED_ORDER_RGB: return "RGB";
        case LED_ORDER_RBG: return "RBG";
        case LED_ORDER_GRB: return "GRB";
        case LED_ORDER_GBR: return "GBR";
        case LED_ORDER_BRG: return "BRG";
        case LED_ORDER_BGR: return "BGR";
        default: return "BRG";
    }
}

bool LedManager::setColorOrderByName(const String& name) {
    if (name == "RGB") _colorOrder = LED_ORDER_RGB;
    else if (name == "RBG") _colorOrder = LED_ORDER_RBG;
    else if (name == "GRB") _colorOrder = LED_ORDER_GRB;
    else if (name == "GBR") _colorOrder = LED_ORDER_GBR;
    else if (name == "BRG") _colorOrder = LED_ORDER_BRG;
    else if (name == "BGR") _colorOrder = LED_ORDER_BGR;
    else return false;
    return true;
}

void LedManager::loadPreferences() {
    _prefs.begin(LED_PREFS_NS, true);
    setLedCount(_prefs.getUShort("count", LED_DEFAULT_COUNT));
    uint8_t savedOrder = _prefs.getUChar("colorOrder", LED_ORDER_BRG);
    _colorOrder = savedOrder <= LED_ORDER_BGR
        ? static_cast<LedColorOrder>(savedOrder)
        : LED_ORDER_BRG;
    _prefs.end();
}

void LedManager::savePreferences() {
    _prefs.begin(LED_PREFS_NS, false);
    _prefs.putUShort("count", _ledCount);
    _prefs.putUChar("colorOrder", static_cast<uint8_t>(_colorOrder));
    _prefs.end();
    Serial.printf("[LED] Prefs saved - count: %u, color order: %s\n",
                  _ledCount, getColorOrderName());
}
