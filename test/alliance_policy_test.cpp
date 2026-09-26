// Compile-only assertions; no executable, sockets, mock arena, or device I/O.
#include "../src/alliance/alliance_policy.h"
using alliance::StopBank;

constexpr bool mappings() {
    const uint8_t red[] = {INPUT_RED_1_ESTOP, INPUT_RED_1_ASTOP, INPUT_RED_2_ESTOP,
                          INPUT_RED_2_ASTOP, INPUT_RED_3_ESTOP, INPUT_RED_3_ASTOP};
    const uint8_t blue[] = {INPUT_BLUE_1_ESTOP, INPUT_BLUE_1_ASTOP, INPUT_BLUE_2_ESTOP,
                           INPUT_BLUE_2_ASTOP, INPUT_BLUE_3_ESTOP, INPUT_BLUE_3_ASTOP};
    for (uint8_t i = 0; i < alliance::InputCount; ++i) {
        if (alliance::channel(false, i) != red[i] || alliance::channel(true, i) != blue[i]) return false;
    }
    return true;
}

constexpr bool bootAndRefresh() {
    StopBank<4> bank;
    StopBank<4>::Candidate item;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) bank.observe(i, true, 0);
    for (uint8_t i = 0; i < alliance::InputCount; ++i) {
        if (!bank.next(1, item) || item.index != i || !item.state || !item.queued) return false;
        if (!bank.acknowledge(item, 1)) return false;
    }
    if (bank.next(100, item)) return false;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) {
        if (!bank.next(101, item) || item.index != i || !item.state || item.queued) return false;
        if (bank.acknowledge(item, 101)) return false;
    }
    return !bank.next(101, item);
}

constexpr bool independentStopsAndRetries() {
    StopBank<8> bank;
    StopBank<8>::Candidate item;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) bank.observe(i, true, 0);
    for (uint8_t i = 0; i < alliance::InputCount; ++i) {
        if (!bank.next(0, item)) return false;
        bank.acknowledge(item, 0);
    }
    bank.observe(0, false, 10);
    bank.observe(0, true, 11);
    if (!bank.next(12, item) || item.index != 0 || item.state) return false;
    auto original = item;
    // Without an ACK, retries keep the original head and sequence.
    if (!bank.next(13, item) || item.event.sequence != original.event.sequence) return false;
    bank.acknowledge(item, 14);
    bank.observe(1, false, 15);
    if (!bank.next(15, item) || item.index != 1 || item.state) return false;
    bank.acknowledge(item, 15);
    // Channel 0 release waits the full hold time; others can still deliver.
    if (bank.next(99, item)) return false;
    if (!bank.next(114, item) || item.index != 2) return false;
    for (uint8_t i = 2; i < alliance::InputCount; ++i) {
        if (!bank.next(114, item) || item.index != i) return false;
        bank.acknowledge(item, 114);
    }
    return bank.next(114, item) && item.index == 0 && item.state && item.queued;
}

constexpr bool overflowStopsWholeAlliance() {
    StopBank<2> bank;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) bank.observe(i, true, 0);
    bank.observe(3, false, 1);
    bank.observe(3, true, 2);
    if (!bank.fault || bank.inputs[3].unretained != 1) return false;
    StopBank<2>::Candidate item;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) {
        if (!bank.next(3, item) || item.index != i || item.state || !item.fault) return false;
        if (bank.acknowledge(item, 3)) return false; // Frozen history cannot drain releases.
    }
    if (bank.next(102, item)) return false;
    bank.observe(0, false, 4);
    return bank.inputs[0].unretained == 1 && bank.inputs[0].count == 1
        && bank.next(103, item) && !item.state;
}

constexpr bool inFlightFaultAndTimeWrap() {
    StopBank<2> bank;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) bank.observe(i, false, 0xfffffff0u);
    StopBank<2>::Candidate item;
    for (uint8_t i = 0; i < alliance::InputCount; ++i) {
        if (!bank.next(0xfffffff0u, item)) return false;
        bank.acknowledge(item, 0xfffffff0u);
    }
    bank.observe(0, true, 0xfffffff1u);
    if (bank.next(83, item)) return false; // 99 ms since assert ACK, across wrap.
    if (!bank.next(84, item) || !item.state || item.index != 0) return false;
    // Overflow after selection must still prevent history removal on ACK.
    bank.observe(0, false, 85);
    bank.observe(0, true, 86);
    if (!bank.fault || bank.acknowledge(item, 87)) return false;
    return bank.inputs[0].count == 2;
}

static_assert(mappings(), "Alliance channel mappings changed");
static_assert(alliance::Pins[0] == 1 && alliance::Pins[1] == 2 && alliance::Pins[2] == 3 &&
              alliance::Pins[3] == 15 && alliance::Pins[4] == 18 && alliance::Pins[5] == 16,
              "Legacy alliance wiring changed");
static_assert(bootAndRefresh(), "All six boot snapshots and refreshes must be delivered fairly");
static_assert(independentStopsAndRetries(), "A held release must not block another input");
static_assert(overflowStopsWholeAlliance(), "Overflow must latch all six inputs in stop");
static_assert(inFlightFaultAndTimeWrap(), "Fault and timestamp rollover handling changed");
