#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../websocket/input_map.h"

namespace fms {
constexpr int StopPin = 33;
constexpr int StartPin = 34;
constexpr int StopChannel = INPUT_FIELD_ESTOP;
constexpr uint32_t RefreshMs = 100;
constexpr uint32_t StopHoldMs = 100;
constexpr uint32_t StartExpiryMs = 500;
constexpr bool wireState(bool gpioHigh) { return !gpioHigh; }
constexpr uint32_t retryDelay(uint8_t failures) {
    return failures >= 5 ? 1000 : (50u << (failures ? failures - 1 : 0));
}
constexpr bool startExpired(uint32_t now, uint32_t pressedAt, bool connected) {
    return !connected || uint32_t(now - pressedAt) > StartExpiryMs;
}
constexpr bool releaseHeld(bool asserted, uint32_t now, uint32_t ackAt) {
    return asserted && uint32_t(now - ackAt) < StopHoldMs;
}
constexpr bool mayStart(bool pending, bool fault, size_t queued, bool released,
                        uint8_t failures, bool holdingStop) {
    return pending && !fault && queued == 0 && released && failures == 0 && !holdingStop;
}
struct Transition {
    uint32_t sequence = 0;
    uint32_t sampledAt = 0;
    bool state = false;
};

// Synchronization belongs to the caller. A failed enqueue never overwrites
// history: it latches fail-stop and counts every unretained transition.
template<size_t Capacity> class StopHistory {
public:
    constexpr void observe(bool state, uint32_t now) {
        if (seen && state == latest) return;
        seen = true;
        latest = state;
        ++observed;
        if (fault || count == Capacity) {
            fault = true;
            ++unretained;
            return;
        }
        entries[(head + count) % Capacity] = {observed, now, state};
        ++count;
        if (count > highWater) highWater = count;
    }
    constexpr bool peek(Transition& out) const {
        if (!count) return false;
        out = entries[head];
        return true;
    }
    constexpr bool acknowledge(uint32_t sequence) {
        if (fault || !count || entries[head].sequence != sequence) return false;
        head = (head + 1) % Capacity;
        --count;
        return true;
    }
    bool seen = false, latest = false, fault = false;
    uint32_t observed = 0, unretained = 0;
    size_t count = 0, highWater = 0;
private:
    Transition entries[Capacity]{};
    size_t head = 0;
};

// Release must be stable before arming, including after boot. A held button
// cannot create another start, and contact bounce cannot re-arm it.
class StartEdge {
public:
    constexpr bool sample(bool pressed, uint32_t now) {
        if (!seen || pressed != raw) {
            seen = true;
            raw = pressed;
            changedAt = now;
        }
        if (uint32_t(now - changedAt) < 30) return false;
        if (!pressed) armed = true;
        else if (armed) {
            armed = false;
            return true;
        }
        return false;
    }
private:
    bool seen = false, raw = false, armed = false;
    uint32_t changedAt = 0;
};
}
