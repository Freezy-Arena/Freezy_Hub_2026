#include "relay_manager.h"

void RelayManager::begin() {
    for (uint8_t i = 0; i < RELAY_MAX; i++) {
        _channels[i].enabled = false;
        _channels[i].state   = false;
    }
    Serial.println("[RELAY] Initialized");
}

void RelayManager::addChannel(uint8_t ch, gpio_num_t pin) {
    if (ch >= RELAY_MAX) {
        Serial.printf("[RELAY] Channel %d out of range\n", ch);
        return;
    }
    _channels[ch].pin     = pin;
    _channels[ch].enabled = true;
    _channels[ch].state   = false;

    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    Serial.printf("[RELAY] Channel %d on GPIO %d ready\n", ch, pin);
}

void RelayManager::setState(uint8_t ch, bool state) {
    if (ch >= RELAY_MAX || !_channels[ch].enabled) return;
    _channels[ch].state = state;
    digitalWrite(_channels[ch].pin, state ? HIGH : LOW);
}

bool RelayManager::getState(uint8_t ch) {
    if (ch >= RELAY_MAX || !_channels[ch].enabled) return false;
    return _channels[ch].state;
}

void RelayManager::toggle(uint8_t ch) {
    if (ch >= RELAY_MAX || !_channels[ch].enabled) return;
    setState(ch, !_channels[ch].state);
}

void RelayManager::allOff() {
    for (uint8_t i = 0; i < RELAY_MAX; i++) {
        if (_channels[i].enabled) setState(i, false);
    }
}