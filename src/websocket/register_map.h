#pragma once
#include <stdint.h>

// ─── PLC Register Index Map ───────────────────────────────────────────────────
// Update this file each season to match the arena's register assignments.
// Usage:  REG_RED_HUB_COUNT_1

enum PlcRegister : uint8_t {
    REG_FIELD_IO_CONNECTION = 0,
    REG_RED_HUB_TOTAL       = 1,
    REG_BLUE_HUB_TOTAL      = 2,
    REG_RED_HUB_COUNT_1     = 3,
    REG_RED_HUB_COUNT_2     = 4,
    REG_RED_HUB_COUNT_3     = 5,
    REG_RED_HUB_COUNT_4     = 6,
    REG_BLUE_HUB_COUNT_1    = 7,
    REG_BLUE_HUB_COUNT_2    = 8,
    REG_BLUE_HUB_COUNT_3    = 9,
    REG_BLUE_HUB_COUNT_4    = 10,

    REG_COUNT               // Always last — total number of registers
};