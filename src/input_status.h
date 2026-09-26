#pragma once
#include <stdint.h>

// A copy of local sampled inputs, never an arena acknowledgment or stop command.
// Producers copy this under their sampling lock; HTTP serialization happens later.
struct InputStatusSnapshot {
    bool sampled = false;
    bool fault = false;
    uint32_t sampledAt = 0;
    uint8_t count = 0;
    bool pressed[6]{};
};

using InputStatusProvider = InputStatusSnapshot (*)();
