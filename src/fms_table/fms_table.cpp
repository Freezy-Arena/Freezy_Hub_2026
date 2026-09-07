#include "fms_table.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

void FmsTable::begin(const String& host, uint16_t port, EthManager& network) {
    _network = &network;
    _baseUrl = "http://" + host + ":" + String(port);
    // Preserve all legacy input pin modes, even the six unused table channels.
    for (int pin : {33, 1, 2, 3, 15, 18, 16}) pinMode(pin, INPUT);
    pinMode(fms::StartPin, INPUT_PULLUP);
    sample();
    BaseType_t sampler = xTaskCreatePinnedToCore(sampleTask, "fms-sample", 3072, this, 3, nullptr, 1);
    BaseType_t sender = xTaskCreatePinnedToCore(deliveryTask, "fms-http", 8192, this, 1, nullptr, 0);
    if (sampler != pdPASS || sender != pdPASS) {
        portENTER_CRITICAL(&_mux);
        _history.fault = true;
        portEXIT_CRITICAL(&_mux);
        Serial.println("[FMS] FAULT: task allocation failed; starts inhibited; delivery may be unavailable");
    }
}

void FmsTable::sampleTask(void* self) {
    auto& table = *static_cast<FmsTable*>(self);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        table.sample();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1) ? pdMS_TO_TICKS(1) : 1);
    }
}

void FmsTable::sample() {
    uint32_t nowUs = micros(), now = millis();
    bool state = fms::wireState(digitalRead(fms::StopPin) == HIGH);
    bool start = digitalRead(fms::StartPin) == LOW;
    portENTER_CRITICAL(&_mux);
    if (_sampleSeen) _status.sampleGapUs = max(_status.sampleGapUs, uint32_t(nowUs - _lastSampleUs));
    _sampleSeen = true;
    _lastSampleUs = nowUs;
    _history.observe(state, now);
    if (_startEdge.sample(start, now)) {
        if (_history.fault || !state || _startPending || !_network->isConnected()) {
            ++_status.startRejected;
        } else {
            _startAt = now;
            _startPending = true;
        }
    }
    // A stop observed before dispatch cancels pending start even if released.
    if (_startPending && (!state || _history.fault || !_network->isConnected())) {
        _startPending = false;
        ++_status.startRejected;
    }
    portEXIT_CRITICAL(&_mux);
}

void FmsTable::deliveryTask(void* self) {
    static_cast<FmsTable*>(self)->deliver();
}

int FmsTable::request(const char* path, const char* body, String& response) {
    uint32_t began = millis();
    HTTPClient http;
    http.setConnectTimeout(250);
    http.setTimeout(500);
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    int status = -1;
    if (http.begin(_baseUrl + path)) {
        if (body) {
            http.addHeader("Content-Type", "application/json");
            status = http.POST(String(body));
        } else status = http.GET();
        // These arena handlers return small, Content-Length-delimited bodies.
        // Bound memory and response reads; unknown/chunked bodies are failures.
        int length = http.getSize();
        if (status > 0 && length >= 0 && length <= 512) {
            char buffer[513];
            http.getStream().setTimeout(500);
            size_t got = http.getStream().readBytes(buffer, length);
            buffer[got] = '\0';
            if (got == size_t(length)) response = String(buffer, got);
            else status = -2;
        } else if (status > 0) status = -3;
    }
    http.end();
    portENTER_CRITICAL(&_mux);
    _status.httpMaxMs = max(_status.httpMaxMs, uint32_t(millis() - began));
    ++_status.httpCount;
    _status.lastHttp = status;
    // Preserve response heartbeat colors, but only a verified 200 ACK retires
    // a stop. Positive HTTP errors used to be incorrectly treated as delivered.
    _status.heartbeat = status > 0 ? 1 : 2;
    if (status != 200) ++_status.failures;
    portEXIT_CRITICAL(&_mux);
    return status;
}

void FmsTable::deliver() {
    uint32_t nextStop = 0, lastRefresh = 0, lastStack = 0, lastAssertAck = 0;
    bool asserted = false;
    uint8_t failures = 0;
    for (;;) {
        uint32_t now = millis();
        bool connected = _network->isConnected();
        fms::Transition event;
        portENTER_CRITICAL(&_mux);
        bool queued = _history.peek(event);
        bool fault = _history.fault;
        bool latest = _history.latest;
        if (_startPending && fms::startExpired(now, _startAt, connected)) {
            _startPending = false;
            ++_status.startRejected;
        }
        portEXIT_CRITICAL(&_mux);
        if (!connected) {
            portENTER_CRITICAL(&_mux);
            _status.heartbeat = 3;
            portEXIT_CRITICAL(&_mux);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Fault takes precedence over retained history. Never replay a release
        // after overflow. History remains frozen for diagnostics until reboot.
        bool sendState = fault ? false : queued ? event.state : latest;
        bool due = (fault || queued || uint32_t(now - lastRefresh) >= fms::RefreshMs)
                   && int32_t(now - nextStop) >= 0;
        if (sendState && fms::releaseHeld(asserted, now, lastAssertAck)) due = false;
        if (due) {
            String response;
            int status = request("/api/freezy/eStopState", sendState
                ? "[{\"channel\":0,\"state\":true}]" : "[{\"channel\":0,\"state\":false}]", response);
            bool ok = fms::stopAcknowledged(status, response == "eStop state updated successfully.");
            uint32_t finished = millis();
            portENTER_CRITICAL(&_mux);
            if (ok) {
                if (!fault && queued && _history.acknowledge(event.sequence)) {
                    ++_status.acks;
                    _status.ackMaxMs = max(_status.ackMaxMs, uint32_t(finished - event.sampledAt));
                }
            } else {
                ++_status.retries;
                if (status == 200) ++_status.failures;
            }
            portEXIT_CRITICAL(&_mux);
            if (ok) {
                failures = 0;
                lastRefresh = finished;
                nextStop = finished + (fault ? fms::RefreshMs : 0);
                asserted = !sendState;
                if (asserted) lastAssertAck = finished;
            } else {
                if (failures < 5) ++failures;
                nextStop = finished + fms::retryDelay(failures);
            }
            // Yield even when draining a large backlog.
            vTaskDelay(1);
            continue;
        }

        portENTER_CRITICAL(&_mux);
        bool start = fms::mayStart(_startPending, _history.fault, _history.count, _history.latest,
                                  failures, fms::releaseHeld(asserted, now, lastAssertAck));
        if (start) _startPending = false;
        portEXIT_CRITICAL(&_mux);
        if (start) {
            String response;
            int status = request("/api/freezy/startMatch", "{\"match\":\"start\"}", response);
            portENTER_CRITICAL(&_mux);
            ++_status.startSent;
            if (status != 200) ++_status.startFailed;
            portEXIT_CRITICAL(&_mux);
            // Never retry an ambiguous start: the endpoint has no idempotency
            // key and returns 200 even if Arena.StartMatch rejects the match.
        } else if (!queued && !fault && uint32_t(now - lastStack) >= 500) {
            String response;
            int status = request("/api/freezy/field_stack_light", nullptr, response);
            JsonDocument doc;
            if (status > 0 && !deserializeJson(doc, response) && doc.is<JsonObject>()) {
                portENTER_CRITICAL(&_mux);
                _status.red = doc["redStackLight"] | false;
                _status.blue = doc["blueStackLight"] | false;
                _status.orange = doc["orangeStackLight"] | false;
                _status.green = doc["greenStackLight"] | false;
                ++_status.stackVersion;
                portEXIT_CRITICAL(&_mux);
            }
            lastStack = millis();
        }
        vTaskDelay(1);
    }
}

void FmsTable::update(LedManager& leds, uint32_t wsMessages) {
    uint32_t now = millis();
    if (_loopSeen) {
        uint32_t gap = now - _lastLoopAt;
        _loopGapMs = max(_loopGapMs, gap);
        if (gap > 200) { _timingWarning = true; _warningAt = now; }
    }
    _loopSeen = true;
    _lastLoopAt = now;
    if (_timingWarning && uint32_t(now - _warningAt) >= 10000) _timingWarning = false;
    portENTER_CRITICAL(&_mux);
    Status status = _status;
    bool fault = _history.fault;
    size_t depth = _history.count, high = _history.highWater;
    uint32_t observed = _history.observed, lost = _history.unretained;
    fms::Transition oldest;
    bool pending = _history.peek(oldest);
    if (uint32_t(now - _lastHeartbeat) >= 500 && !_heartbeatOn) _status.heartbeat = 0;
    portEXIT_CRITICAL(&_mux);
    if (status.stackVersion != _stackVersion) {
        _stackVersion = status.stackVersion;
        auto fill = [&](int start, int length, bool on, CRGB color) {
            for (int i = start; i < start + length; ++i) leds.setLedRaw(i, on ? color : CRGB::Black);
        };
        fill(3, 60, status.red, CRGB(255, 0, 0));
        fill(60, 60, status.blue, CRGB(0, 0, 255)); // Legacy overlap: blue wins 60..62.
        fill(120, 60, status.orange, CRGB(150, 100, 0));
        fill(180, 56, status.green, CRGB(0, 255, 0));
    }
    if (uint32_t(now - _lastHeartbeat) >= 500) {
        _lastHeartbeat = now;
        _heartbeatOn = !_heartbeatOn;
        CRGB color = status.heartbeat == 1 ? CRGB(50, 50, 50)
                   : status.heartbeat == 2 ? CRGB::Orange
                   : status.heartbeat == 3 ? CRGB::Red : CRGB::Black;
        if (_timingWarning && status.heartbeat == 1) color = CRGB::Red;
        leds.setLedRaw(0, _heartbeatOn ? color : CRGB::Black);
    }
    leds.setLedRaw(1, fault ? CRGB::Red : CRGB(255, 0, 255));
    leds.setLedRaw(2, wsMessages % 2 ? CRGB(0, 50, 0) : CRGB::Black);
    uint32_t ledStart = millis();
    leds.show();
    _ledMaxMs = max(_ledMaxMs, uint32_t(millis() - ledStart));
    if (uint32_t(now - _lastReport) >= 5000) {
        _lastReport = now;
        Serial.printf("[FMS TIMING] sample_gap_us=%lu loop_gap_ms=%lu led_max_ms=%lu http_max_ms=%lu ack_max_ms=%lu queue=%u high=%u oldest_ms=%lu observed=%lu ack=%lu unretained=%lu FAULT=%d http=%lu fail=%lu retry=%lu last_http=%d start_sent=%lu start_rejected=%lu start_failed=%lu\n",
            status.sampleGapUs, _loopGapMs, _ledMaxMs, status.httpMaxMs, status.ackMaxMs, unsigned(depth), unsigned(high),
            pending ? uint32_t(now - oldest.sampledAt) : 0, observed, status.acks, lost, fault,
            status.httpCount, status.failures, status.retries, status.lastHttp,
            status.startSent, status.startRejected, status.startFailed);
    }
}
