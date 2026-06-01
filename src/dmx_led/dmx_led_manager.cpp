#include "dmx_led_manager.h"

DmxLedManager::DmxLedManager(LedManager& leds, RoleManager& role)
    : _leds(leds), _role(role) {}

void DmxLedManager::begin() {
    _udp.begin(SACN_PORT);
    Serial.printf("[DMX] Listening on UDP port %d universe %d\n",
                  SACN_PORT, SACN_UNIVERSE);
}

void DmxLedManager::update() {
    int size = _udp.parsePacket();
    if (size > 0) {
        size = _udp.read(_packet, SACN_MAX_PACKET_SIZE);
        if (_validatePacket(size)) {
            _processPixels(size);
            _lastPacket = millis();
            _receiving  = true;
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

bool DmxLedManager::_validatePacket(int size) {
    if (size < SACN_PIXEL_DATA_OFFSET) {
        Serial.printf("[DMX] Packet too small: %d bytes\n", size);
        return false;
    }

    const uint8_t acnId[] = {
        0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e,
        0x31, 0x37, 0x00, 0x00, 0x00
    };
    for (int i = 0; i < 12; i++) {
        if (_packet[4 + i] != acnId[i]) {
            Serial.println("[DMX] Invalid ACN identifier");
            return false;
        }
    }

    int universe = (_packet[113] << 8) | _packet[114];
    if (universe != SACN_UNIVERSE) {
        return false;
    }

    uint8_t seq = _packet[111];
    if (seq != 0 && seq <= _lastSequence && (_lastSequence - seq) < 128) {
        Serial.printf("[DMX] Out of order packet seq=%d last=%d\n",
                      seq, _lastSequence);
        return false;
    }
    _lastSequence = seq;

    return true;
}

void DmxLedManager::_processPixels(int size) {
    const uint8_t* pixelData = _packet + SACN_PIXEL_DATA_OFFSET;
    int pixelBytes           = size - SACN_PIXEL_DATA_OFFSET;
    int numPixels            = pixelBytes / 3;

    Serial.printf("[DMX] Packet size: %d  pixelBytes: %d  numPixels: %d\n",
                  size, pixelBytes, numPixels);
                  
    int endPixel = min(numPixels, NUM_LEDS);

    if (_role.getRole() == ROLE_BLUE_HUB) {
        int offset   = (numPixels / 2) * 3;
        pixelData   += offset;
        pixelBytes  -= offset;
        numPixels    = pixelBytes / 3;
        endPixel     = min(numPixels, NUM_LEDS);
    }

    for (int i = 0; i < endPixel; i++) {
        uint8_t r = pixelData[i * 3 + 0];
        uint8_t g = pixelData[i * 3 + 1];
        uint8_t b = pixelData[i * 3 + 2];
        _leds.setLedRaw(i, CRGB(r, g, b));
    }
    _leds.show();
}