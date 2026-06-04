#pragma once
#include <Arduino.h>
#include <WiFiUDP.h>
#include <ETH.h>
#include "../led/led_manager.h"
#include "../role_config.h"

#define SACN_UNIVERSE_RED   1
#define SACN_UNIVERSE_BLUE  2

#define SACN_PORT               5568
#define SACN_UNIVERSE           1
#define SACN_PIXEL_DATA_OFFSET  126
#define SACN_TIMEOUT_MS         3000
#define SACN_MAX_PACKET_SIZE    (SACN_PIXEL_DATA_OFFSET + (300 * 3))

class DmxLedManager {
public:
    DmxLedManager(LedManager& leds, RoleManager& role);

    void begin();
    void update();
    bool isReceiving();

private:
    LedManager&     _leds;
    RoleManager&    _role;
    WiFiUDP         _udp;

    uint8_t         _packet[SACN_MAX_PACKET_SIZE];
    uint32_t        _lastPacket     = 0;
    bool            _receiving      = false;
    uint8_t         _lastSequence   = 0;
    uint8_t     _packetRed[SACN_MAX_PACKET_SIZE];
    uint8_t     _packetBlue[SACN_MAX_PACKET_SIZE];
    int         _srcPixelsRed   = 0;
    int         _srcPixelsBlue  = 0;
    uint8_t     _lastSeqRed     = 0;
    uint8_t     _lastSeqBlue    = 0;

    bool    _validatePacket(uint8_t* packet, int size, int expectedUniverse, uint8_t& lastSeq);
    void    _renderPixels();    // Combines both universes and writes to LEDs

/*     bool    _validatePacket(int size);
    void    _processPixels(int size);
    void    x_processPixels(int size); */
};