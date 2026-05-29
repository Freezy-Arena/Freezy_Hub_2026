#pragma once
#include <Arduino.h>
#include "driver/pcnt.h"

#define COUNTER_MAX_UNITS   4

struct CounterChannel {
    gpio_num_t      pin;
    pcnt_unit_t     unit;
    bool            enabled;
    int64_t         accumulated;
    int16_t         lastRaw;
};

class CounterManager {
public:
    void begin();
    void update();

    void        addChannel(uint8_t ch, gpio_num_t pin);
    int64_t     getCount(uint8_t ch);
    void        resetCount(uint8_t ch);
    void        resetAll();
    bool        isEnabled(uint8_t ch);

private:
    CounterChannel _channels[COUNTER_MAX_UNITS];
    void _initUnit(uint8_t ch);

    static void IRAM_ATTR _pcntIsr(void* arg);
    static int64_t _overflow[COUNTER_MAX_UNITS];
    static bool _isrInstalled;
};