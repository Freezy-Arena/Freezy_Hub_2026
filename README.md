# ESP32-S3 FRC Arena PLC Simulator

A modular ESP32-S3 firmware project that simulates a PLC for FRC (FIRST Robotics Competition) arena hardware. The device connects to the arena server via WebSocket, reports counter inputs as PLC registers, and responds to coil state changes to drive physical outputs.

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-S3 DevKitM-1 |
| Ethernet | W5500 SPI module |
| LED Strip | WS2812B, 300 LEDs, GPIO 38 |
| Relay Output | GPIO 33, GPIO 34 |
| Counter Inputs | GPIO 15, 1, 2, 3 (PCNT hardware counters) |

### W5500 SPI Wiring

| Signal | GPIO |
|---|---|
| SCK | 13 |
| MISO | 12 |
| MOSI | 11 |
| CS | 14 |
| IRQ | 10 |
| RST | 9 |

---

## Features

- **Hardware PCNT counters** — 4 channels, 64-bit accumulation, overflow-safe
- **Individually addressable RGB LEDs** — FastLED, non-blocking animations
- **Relay outputs** — up to 4 channels
- **Wired Ethernet** — W5500 via SPI, DHCP or static IP
- **WebSocket client** — connects to arena server, auto-reconnects on disconnect
- **PLC register reporting** — sends counter values as PLC registers each 500ms
- **PLC coil handling** — reacts to coil state changes (motor, light, reset, stack lights)
- **Device roles** — `redHub` or `blueHub`, changes register map and coil assignments
- **Web configuration UI** — dark-themed page to set network and role, saved to flash
- **Preferences** — all settings persist across reboots via ESP32 NVS flash

---

## Project Structure

```
src/
├── main.cpp                  # Setup, loop, coil callback
├── role_config.h             # Device roles, register/coil/pin assignments
├── led/
│   ├── led_manager.h
│   └── led_manager.cpp       # FastLED wrapper, animations
├── counter/
│   ├── counter_manager.h
│   └── counter_manager.cpp   # PCNT hardware counter manager
├── relay/
│   ├── relay_manager.h
│   └── relay_manager.cpp     # GPIO relay output manager
├── network/
│   ├── network_manager.h
│   └── network_manager.cpp   # Ethernet (W5500), DHCP/static, preferences
├── webserver/
│   ├── web_manager.h
│   └── web_manager.cpp       # Async web config UI
└── websocket/
    ├── ws_manager.h
    ├── ws_manager.cpp        # WebSocket client, register/coil handling
    ├── coil_map.h            # PlcCoil enum — update each season
    ├── register_map.h        # PlcRegister enum — update each season
    └── input_map.h           # PlcInput enum — update each season
```

---

## Configuration

### Web UI

Once the device has an IP address, open a browser to `http://<device-ip>` to configure:

- **Network** — toggle DHCP or set a static IP and gateway
- **Device Role** — select `redHub` or `blueHub`

Settings are saved to flash and the device reboots automatically.

### Season Updates

Three files need updating each season to match the arena's PLC assignments:

| File | Contents |
|---|---|
| `src/websocket/coil_map.h` | `PlcCoil` enum — coil index assignments |
| `src/websocket/register_map.h` | `PlcRegister` enum — register index assignments |
| `src/websocket/input_map.h` | `PlcInput` enum — input index assignments |
| `src/role_config.h` | Role definitions — register/coil mappings per role |

---

## Device Roles

| Role | Registers | Total | Motor Coil | Light Coil |
|---|---|---|---|---|
| `redHub` | Reg 3, 4, 5, 6 | Reg 1 | `COIL_RED_HUB_MOTOR` | `COIL_RED_HUB_LIGHT` |
| `blueHub` | Reg 7, 8, 9, 10 | Reg 2 | `COIL_BLUE_HUB_MOTOR` | `COIL_BLUE_HUB_LIGHT` |

---

## WebSocket Protocol

Connects to `ws://<arenaHost>:<arenaPort>/api/plc/websocket`

### Outbound — Register Update
```json
{
  "type": "setRegisters",
  "data": [
    { "register": 3, "cValue": 42 },
    { "register": 1, "cValue": 42 }
  ]
}
```

### Inbound — Coil State Change
```json
{
  "type": "plcIoChange",
  "data": {
    "Coils": [false, true, false, false, false, false, false, false]
  }
}
```

---

## Dependencies

Defined in `platformio.ini`:

| Library | Purpose |
|---|---|
| `FastLED` | WS2812B LED control |
| `ESPAsyncWebServer` | Async web configuration server |
| `AsyncTCP` | Required by ESPAsyncWebServer |
| `links2004/WebSockets` | WebSocket client |
| `bblanchon/ArduinoJson` | JSON encode/decode |
| `arduino-libraries/Ethernet` | Ethernet support |

---

## Build

This project uses [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### platformio.ini

```ini
[env:esp32-s3-devkitm-1]
platform = espressif32
board = esp32-s3-devkitm-1
framework = arduino
monitor_speed = 115200
lib_deps =
    arduino-libraries/Ethernet@^2.0.2
    https://github.com/bblanchon/ArduinoJson.git
    FastLED
    ESPAsyncWebServer
    AsyncTCP
    links2004/WebSockets@^2.4.1
platform_packages =
    framework-arduinoespressif32 @ https://github.com/espressif/arduino-esp32.git#3.0.2
    framework-arduinoespressif32-libs @ https://github.com/espressif/arduino-esp32/releases/download/3.0.2/esp32-arduino-libs-3.0.2.zip
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
    -DESP32_S3_DEVKITM_1
lib_compat_mode = strict
```

---

## Debug Output

Set `DEBUG_SERIAL` in `main.cpp` to control serial logging:

```cpp
#define DEBUG_SERIAL true    // Set false to silence all Serial output
```

---

## TODO

- [ ] Arena server input endpoint — `sendInputs()` is implemented, message type TBD with arena team
- [ ] GPIO → PlcInput role mapping for physical sensor reporting
- [ ] Webserver config for arena host/port
- [ ] Additional device roles

---

## License

MIT
