#pragma once
#include <Arduino.h>

#define RELAY_MAX   4

struct RelayChannel {
    gpio_num_t  pin;
    bool        state;
    bool        enabled;
};

class RelayManager {
public:
    void begin();
    void addChannel(uint8_t ch, gpio_num_t pin);
    void setState(uint8_t ch, bool state);
    bool getState(uint8_t ch);
    void toggle(uint8_t ch);
    void allOff();


private:
    RelayChannel _channels[RELAY_MAX];
};