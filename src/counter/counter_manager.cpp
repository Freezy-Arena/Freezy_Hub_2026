#include "counter_manager.h"

// Static members
int64_t CounterManager::_overflow[COUNTER_MAX_UNITS] = {0};
bool    CounterManager::_isrInstalled = false;

// PCNT overflows at +32767 / -32768 — ISR accumulates the overflow
void IRAM_ATTR CounterManager::_pcntIsr(void* arg) {
    uint32_t status;
    pcnt_get_event_status((pcnt_unit_t)(uint32_t)arg, &status);

    uint8_t unit = (uint8_t)(uint32_t)arg;
    if (status & PCNT_EVT_H_LIM) _overflow[unit] += 32767;
    if (status & PCNT_EVT_L_LIM) _overflow[unit] -= 32768;
}

void CounterManager::begin() {
    // Initialize all channel structs as disabled
    for (uint8_t i = 0; i < COUNTER_MAX_UNITS; i++) {
        _channels[i].enabled     = false;
        _channels[i].accumulated = 0;
        _channels[i].lastRaw     = 0;
        _channels[i].unit        = (pcnt_unit_t)i;
    }

    Serial.println("[COUNTER] Initialized");
}

void CounterManager::addChannel(uint8_t ch, gpio_num_t pin) {
    if (ch >= COUNTER_MAX_UNITS) {
        Serial.printf("[COUNTER] Channel %d out of range\n", ch);
        return;
    }

    _channels[ch].pin     = pin;
    _channels[ch].enabled = true;
    _initUnit(ch);

    Serial.printf("[COUNTER] Channel %d on GPIO %d ready\n", ch, pin);
}

void CounterManager::_initUnit(uint8_t ch) {
    pcnt_config_t cfg = {
        .pulse_gpio_num = _channels[ch].pin,
        .ctrl_gpio_num  = PCNT_PIN_NOT_USED,
        .lctrl_mode     = PCNT_MODE_KEEP,
        .hctrl_mode     = PCNT_MODE_KEEP,
        .pos_mode       = PCNT_COUNT_INC,   // Count rising edges
        .neg_mode       = PCNT_COUNT_DIS,   // Ignore falling edges
        .counter_h_lim  = 32767,
        .counter_l_lim  = -32768,
        .unit           = _channels[ch].unit,
        .channel        = PCNT_CHANNEL_0,
    };

    pcnt_unit_config(&cfg);

    // Filter out glitches < ~1µs (80MHz clock, value in APB cycles)
    pcnt_set_filter_value(_channels[ch].unit, 100);
    pcnt_filter_enable(_channels[ch].unit);

    // Set up overflow events
    pcnt_event_enable(_channels[ch].unit, PCNT_EVT_H_LIM);
    pcnt_event_enable(_channels[ch].unit, PCNT_EVT_L_LIM);

    // Install ISR once, then add handlers per unit
    if (!_isrInstalled) {
        pcnt_isr_service_install(0);
        _isrInstalled = true;
    }
    pcnt_isr_handler_add(_channels[ch].unit, _pcntIsr,
                         (void*)(uint32_t)_channels[ch].unit);

    pcnt_counter_pause(_channels[ch].unit);
    pcnt_counter_clear(_channels[ch].unit);
    pcnt_counter_resume(_channels[ch].unit);
}

void CounterManager::update() {
    // Poll raw hardware counters — needed if you want smooth overflow tracking
    // without relying solely on the ISR edge case near the limit
    for (uint8_t i = 0; i < COUNTER_MAX_UNITS; i++) {
        if (!_channels[i].enabled) continue;
        int16_t raw;
        pcnt_get_counter_value(_channels[i].unit, &raw);
        _channels[i].lastRaw = raw;
    }
}

int64_t CounterManager::getCount(uint8_t ch) {
    if (ch >= COUNTER_MAX_UNITS || !_channels[ch].enabled) return 0;
    return _overflow[ch] + _channels[ch].lastRaw;
}

void CounterManager::resetCount(uint8_t ch) {
    if (ch >= COUNTER_MAX_UNITS || !_channels[ch].enabled) return;
    _overflow[ch] = 0;
    pcnt_counter_clear(_channels[ch].unit);
    _channels[ch].lastRaw = 0;
    Serial.printf("[COUNTER] Channel %d reset\n", ch);
}

void CounterManager::resetAll() {
    for (uint8_t i = 0; i < COUNTER_MAX_UNITS; i++) {
        if (_channels[i].enabled) resetCount(i);
    }
}

bool CounterManager::isEnabled(uint8_t ch) {
    if (ch >= COUNTER_MAX_UNITS) return false;
    return _channels[ch].enabled;
}