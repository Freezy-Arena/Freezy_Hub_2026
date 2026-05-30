#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "websocket/coil_map.h"
#include "websocket/register_map.h"
#include "websocket/input_map.h"

#define ROLE_PREFS_NS "role"

enum DeviceRole : uint8_t {
    ROLE_RED_HUB  = 0,
    ROLE_BLUE_HUB = 1
};

// Register assignments per role
struct RoleConfig {
    DeviceRole  role;
    const char* name;

    // Registers to write counter values into
    uint8_t     regCh0;       // Counter 0 register
    uint8_t     regCh1;       // Counter 1 register
    uint8_t     regCh2;       // Counter 2 register
    uint8_t     regCh3;       // Counter 3 register
    uint8_t     regTotal;     // Sum register

    // Coils this role cares about
    uint8_t     coilMotor;
    uint8_t     coilLight;

    // PLC Mapping inputs this role cares about (for future use)
    uint8_t     plcInputSensor1;
    uint8_t     plcInputSensor2;
    uint8_t     plcInputSensor3;
    uint8_t     plcInputSensor4;

     // Counter GPIO pins
    gpio_num_t  counterPin0;
    gpio_num_t  counterPin1;
    gpio_num_t  counterPin2;
    gpio_num_t  counterPin3;

    // Relay GPIO pins
    gpio_num_t  relayMotor;
    gpio_num_t  relayLight;
};

// ─── Role definitions ─────────────────────────────────────────────────────────
// Update register/coil assignments here each season.

static const RoleConfig ROLE_CONFIGS[] = {
    {
        ROLE_RED_HUB,  "redHub",
        REG_RED_HUB_COUNT_1,
        REG_RED_HUB_COUNT_2,
        REG_RED_HUB_COUNT_3,
        REG_RED_HUB_COUNT_4,
        REG_RED_HUB_TOTAL,            // Total register (Reg 1 = sum of 3,4,5,6)
        COIL_RED_HUB_MOTOR,
        COIL_RED_HUB_LIGHT,
        INPUT_RED_HUB_SENSOR_1,
        INPUT_RED_HUB_SENSOR_2, 
        INPUT_RED_HUB_SENSOR_3,
        INPUT_RED_HUB_SENSOR_4,
        GPIO_NUM_15, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3,    // Counter pins
        GPIO_NUM_33, GPIO_NUM_34                             // Relay pins
    },
    {
        ROLE_BLUE_HUB, "blueHub",
        REG_BLUE_HUB_COUNT_1,
        REG_BLUE_HUB_COUNT_2,
        REG_BLUE_HUB_COUNT_3,
        REG_BLUE_HUB_COUNT_4,
        REG_BLUE_HUB_TOTAL,           // Total register (Reg 2 = sum of 7,8,9,10)
        COIL_BLUE_HUB_MOTOR,
        COIL_BLUE_HUB_LIGHT,
        INPUT_BLUE_HUB_SENSOR_1,
        INPUT_BLUE_HUB_SENSOR_2,
        INPUT_BLUE_HUB_SENSOR_3,
        INPUT_BLUE_HUB_SENSOR_4,
        GPIO_NUM_15, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3,    // Counter pins
        GPIO_NUM_33, GPIO_NUM_34                             // Relay pins

    }
};

// ─── Role manager ─────────────────────────────────────────────────────────────

class RoleManager {
public:
    void begin() {
        loadPreferences();
        Serial.printf("[ROLE] Active role: %s\n", getConfig().name);
    }

    DeviceRole getRole() { return _role; }

    const RoleConfig& getConfig() {
        return ROLE_CONFIGS[_role];
    }

    void setRole(DeviceRole role) {
        _role = role;
        savePreferences();
        Serial.printf("[ROLE] Role changed to: %s\n", getConfig().name);
    }

    void setRoleByName(const String& name) {
        for (auto& cfg : ROLE_CONFIGS) {
            if (name.equalsIgnoreCase(cfg.name)) {
                setRole(cfg.role);
                return;
            }
        }
        Serial.printf("[ROLE] Unknown role '%s' — keeping current\n", name.c_str());
    }

    String getRoleName() {
        return String(getConfig().name);
    }

    void loadPreferences() {
        _prefs.begin(ROLE_PREFS_NS, true);
        _role = (DeviceRole)_prefs.getUChar("role", ROLE_RED_HUB);
        _prefs.end();
    }

    void savePreferences() {
        _prefs.begin(ROLE_PREFS_NS, false);
        _prefs.putUChar("role", (uint8_t)_role);
        _prefs.end();
    }

private:
    DeviceRole  _role = ROLE_RED_HUB;
    Preferences _prefs;
};