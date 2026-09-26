#pragma once
#include "../fms_table/stop_policy.h"

namespace alliance {
constexpr uint8_t InputCount = 6;
// Legacy Freezy Estops wiring: station 1 E/A, station 2 E/A, station 3 E/A.
constexpr uint8_t Pins[InputCount] = {1, 2, 3, 15, 18, 16};
constexpr uint8_t channel(bool blue, uint8_t index) {
    return (blue ? INPUT_BLUE_1_ESTOP : INPUT_RED_1_ESTOP) + index;
}

// Caller synchronizes sampling and delivery. Independent histories preserve
// each input's order without letting a held release block another station.
template<size_t Capacity> class StopBank {
public:
    struct Candidate {
        uint8_t index = 0;
        fms::Transition event;
        bool queued = false, fault = false, state = false;
    };
    fms::StopHistory<Capacity> inputs[InputCount];
    bool fault = false;

    constexpr void observe(uint8_t index, bool state, uint32_t now) {
        if (fault) inputs[index].fault = true;
        inputs[index].observe(state, now);
        if (inputs[index].fault) fault = true;
    }

    constexpr bool next(uint32_t now, Candidate& out) const {
        for (uint8_t offset = 0; offset < InputCount; ++offset) {
            uint8_t i = (cursor + offset) % InputCount;
            Candidate candidate;
            candidate.index = i;
            candidate.queued = inputs[i].peek(candidate.event);
            candidate.fault = fault;
            candidate.state = fault ? false : candidate.queued ? candidate.event.state : inputs[i].latest;
            bool refreshDue = !delivered[i] || uint32_t(now - refreshedAt[i]) >= fms::RefreshMs;
            if (fault ? !refreshDue : (!candidate.queued && !refreshDue)) continue;
            if (candidate.state && fms::releaseHeld(asserted[i], now, assertedAt[i])) continue;
            out = candidate;
            return true;
        }
        return false;
    }

    // Failed/ambiguous delivery must never call this method or consume history.
    constexpr bool acknowledge(const Candidate& item, uint32_t now) {
        uint8_t i = item.index;
        bool popped = !fault && !item.fault && item.queued && inputs[i].acknowledge(item.event.sequence);
        delivered[i] = true;
        refreshedAt[i] = now;
        asserted[i] = !item.state;
        if (asserted[i]) assertedAt[i] = now;
        cursor = (i + 1) % InputCount;
        return popped;
    }
private:
    uint8_t cursor = 0;
    bool delivered[InputCount]{}, asserted[InputCount]{};
    uint32_t refreshedAt[InputCount]{}, assertedAt[InputCount]{};
};
}
