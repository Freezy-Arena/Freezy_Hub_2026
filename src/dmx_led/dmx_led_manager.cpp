#include "dmx_led_manager.h"

DmxLedManager::DmxLedManager(LedManager& leds, RoleManager& role)
    : _leds(leds), _role(role) {}

void DmxLedManager::begin() {
    _udp.begin(SACN_PORT);
    Serial.printf("[DMX] Listening on UDP port %d universes %d and %d\n",
                  SACN_PORT, SACN_UNIVERSE_RED, SACN_UNIVERSE_BLUE);
}

void DmxLedManager::update() {
    int size = _udp.parsePacket();
    if (size > 0) {
        // Peek at universe before deciding which buffer to read into
        uint8_t peek[115];
        _udp.read(peek, 115);
        int universe = (peek[113] << 8) | peek[114];

        // Re-read full packet into correct buffer
        // Note: We already consumed 115 bytes so read remaining into temp
        // Instead read entire packet fresh — use a temp buffer first
        // Actually WiFiUDP doesn't support re-reading, so read into temp then copy
        static uint8_t temp[SACN_MAX_PACKET_SIZE];
        memcpy(temp, peek, 115);
        int remaining = size - 115;
        if (remaining > 0) {
            _udp.read(temp + 115, remaining);
        }

        if (universe == SACN_UNIVERSE_RED) {
            if (_validatePacket(temp, size, SACN_UNIVERSE_RED, _lastSeqRed)) {
                memcpy(_packetRed, temp, size);
                _srcPixelsRed = (size - SACN_PIXEL_DATA_OFFSET) / 3;
                Serial.printf("[DMX] Received universe=%d seq=%u bytes=%d pixels=%d\n",
                              universe, (unsigned)temp[111], size, _srcPixelsRed);
                _lastPacket   = millis();
                _receiving    = true;
                _renderPixels();
            }
        } else if (universe == SACN_UNIVERSE_BLUE) {
            if (_validatePacket(temp, size, SACN_UNIVERSE_BLUE, _lastSeqBlue)) {
                memcpy(_packetBlue, temp, size);
                _srcPixelsBlue = (size - SACN_PIXEL_DATA_OFFSET) / 3;
                Serial.printf("[DMX] Received universe=%d seq=%u bytes=%d pixels=%d\n",
                              universe, (unsigned)temp[111], size, _srcPixelsBlue);
                _lastPacket    = millis();
                _receiving     = true;
                _renderPixels();
            }
        }
    }

    if (_receiving && millis() - _lastPacket > SACN_TIMEOUT_MS) {
        _receiving = false;
        Serial.println("[DMX] Timeout — falling back to coil logic");
    }
}

bool DmxLedManager::isReceiving() {
    return _receiving;
}

bool DmxLedManager::_validatePacket(uint8_t* packet, int size,
                                     int expectedUniverse, uint8_t& lastSeq) {
    if (size < SACN_PIXEL_DATA_OFFSET) {
        Serial.printf("[DMX] Packet too small: %d\n", size);
        return false;
    }

    const uint8_t acnId[] = {
        0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e,
        0x31, 0x37, 0x00, 0x00, 0x00
    };
    for (int i = 0; i < 12; i++) {
        if (packet[4 + i] != acnId[i]) {
            Serial.println("[DMX] Invalid ACN identifier");
            return false;
        }
    }

    int universe = (packet[113] << 8) | packet[114];
    if (universe != expectedUniverse) return false;

    uint8_t seq = packet[111];
    if (seq != 0 && seq <= lastSeq && (lastSeq - seq) < 128) {
        Serial.printf("[DMX] Out of order seq=%d last=%d\n", seq, lastSeq);
        return false;
    }
    lastSeq = seq;

    return true;
}

void DmxLedManager::_renderPixels() {
    // Red universe fills first half of strip
    // Blue universe fills second half of strip
    // Cheesy now uses a single Universe
    int halfStrip = _leds.getLedCount();

    // Red — scale srcPixelsRed into pixels 0 to halfStrip-1
    if (_srcPixelsRed > 0) {
        const uint8_t* pixelData = _packetRed + SACN_PIXEL_DATA_OFFSET;
        for (int i = 0; i < halfStrip; i++) {
            int srcIndex = constrain((i * _srcPixelsRed) / halfStrip,
                                     0, _srcPixelsRed - 1);
            _leds.setLedRaw(i, CRGB(
                pixelData[srcIndex * 3 + 0],
                pixelData[srcIndex * 3 + 1],
                pixelData[srcIndex * 3 + 2]
            ));
        }
    }

    // Blue — scale srcPixelsBlue into pixels after the red range when available
    if (_srcPixelsBlue > 0) {
        const uint8_t* pixelData = _packetBlue + SACN_PIXEL_DATA_OFFSET;
        for (int i = 0; i < halfStrip; i++) {
            int srcIndex = constrain((i * _srcPixelsBlue) / halfStrip,
                                     0, _srcPixelsBlue - 1);
            _leds.setLedRaw(halfStrip + i, CRGB(
                pixelData[srcIndex * 3 + 0],
                pixelData[srcIndex * 3 + 1],
                pixelData[srcIndex * 3 + 2]
            ));
        }
    }

    _leds.show();
}
