#pragma once
#include <Arduino.h>
#include "stop_policy.h"
#include "../network/network_manager.h"
#include "../led/led_manager.h"

class FmsTable {
public:
    // Immutable destination for this boot; configuration changes require reboot.
    void begin(const String& host, uint16_t port, EthManager& network);
    void update(LedManager& leds, uint32_t wsMessages);
private:
    static void sampleTask(void* self);
    static void deliveryTask(void* self);
    void sample();
    void deliver();
    int request(const char* path, const char* body, String& response);
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    fms::StopHistory<256> _history;
    fms::StartEdge _startEdge;
    EthManager* _network = nullptr;
    String _baseUrl;
    struct Status {
        uint32_t sampleGapUs = 0, httpMaxMs = 0, ackMaxMs = 0;
        uint32_t httpCount = 0, failures = 0, retries = 0, acks = 0;
        uint32_t startSent = 0, startRejected = 0, startFailed = 0;
        uint32_t stackVersion = 0;
        int lastHttp = 0;
        uint8_t heartbeat = 0;
        bool red = false, blue = false, orange = false, green = false;
    } _status;
    bool _startPending = false;
    uint32_t _startAt = 0, _lastSampleUs = 0;
    bool _sampleSeen = false;
    uint32_t _lastReport = 0, _lastHeartbeat = 0, _stackVersion = 0;
    bool _heartbeatOn = false;
    uint32_t _lastLoopAt = 0, _loopGapMs = 0, _ledMaxMs = 0, _warningAt = 0;
    bool _loopSeen = false, _timingWarning = false;
};
