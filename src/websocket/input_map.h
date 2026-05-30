#pragma once
#include <stdint.h>

// ─── PLC Input Index Map ──────────────────────────────────────────────────────
// Update this file each season to match the arena's input assignments.
// Usage:  inputs[INPUT_RED_HUB_SENSOR_1]

enum PlcInput : uint8_t {
    INPUT_FIELD_ESTOP       = 0,
    INPUT_RED_1_ESTOP       = 1,
    INPUT_RED_1_ASTOP       = 2,
    INPUT_RED_2_ESTOP       = 3,
    INPUT_RED_2_ASTOP       = 4,
    INPUT_RED_3_ESTOP       = 5,
    INPUT_RED_3_ASTOP       = 6,
    INPUT_BLUE_1_ESTOP      = 7,
    INPUT_BLUE_1_ASTOP      = 8,
    INPUT_BLUE_2_ESTOP      = 9,
    INPUT_BLUE_2_ASTOP      = 10,
    INPUT_BLUE_3_ESTOP      = 11,
    INPUT_BLUE_3_ASTOP      = 12,
    INPUT_RED_CONNECTED_1   = 13,
    INPUT_RED_CONNECTED_2   = 14,
    INPUT_RED_CONNECTED_3   = 15,
    INPUT_BLUE_CONNECTED_1  = 16,
    INPUT_BLUE_CONNECTED_2  = 17,
    INPUT_BLUE_CONNECTED_3  = 18,
    INPUT_FTA_READY         = 19,
    INPUT_RED_HUB_SENSOR_1  = 20,
    INPUT_RED_HUB_SENSOR_2  = 21,
    INPUT_RED_HUB_SENSOR_3  = 22,
    INPUT_RED_HUB_SENSOR_4  = 23,
    INPUT_BLUE_HUB_SENSOR_1 = 24,
    INPUT_BLUE_HUB_SENSOR_2 = 25,
    INPUT_BLUE_HUB_SENSOR_3 = 26,
    INPUT_BLUE_HUB_SENSOR_4 = 27,

    INPUT_COUNT             // Always last — total number of inputs
};