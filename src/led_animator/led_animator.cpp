#include "led_animator.h"

// Fill directions — mirrors strip.go
#define FILL_CENTER_OUT     0
#define FILL_LEFT_TO_RIGHT  1
#define FILL_RIGHT_TO_LEFT  2
#define FILL_EDGES_IN       3

// Strip geometry — mirrors strip.go
#define NUM_SIDES           4
#define FIXTURES_PER_SIDE   2
#define NODES_PER_FIXTURE   8
// numPixels = 4 * 2 * 8 = 64 source pixels, scaled to active LED count

// Startup side delays — mirrors strip.go
#define STARTUP_SIDE1_DELAY     0
#define STARTUP_SIDE24_DELAY    33
#define STARTUP_SIDE3_DELAY     66

LedAnimator::LedAnimator(LedManager& leds, RoleManager& role)
    : _leds(leds), _role(role) {}

void LedAnimator::begin() {
    Serial.println("[ANIM] Initialized");
}

void LedAnimator::setMode(LedMode redMode, LedMode blueMode) {
    LedMode newMode = (_role.getRole() == ROLE_RED_HUB) ? redMode : blueMode;
    LedMode current = activeMode();

    if (newMode != current) {
        _counter = 0;   // Reset counter on mode change, mirrors Go behaviour
        Serial.printf("[ANIM] Mode changed to %d\n", newMode);
    }

    _redMode  = redMode;
    _blueMode = blueMode;
}

LedMode LedAnimator::activeMode() {
    return (_role.getRole() == ROLE_RED_HUB) ? _redMode : _blueMode;
}

void LedAnimator::update() {
    if (millis() - _lastUpdate < ANIM_UPDATE_MS) return;
    _lastUpdate = millis();

    _renderMode(activeMode());
    _counter++;
}

// ─── Mode dispatcher ──────────────────────────────────────────────────────────

void LedAnimator::_renderMode(LedMode mode) {
    switch (mode) {
        case LED_MODE_OFF:              _renderSolid(CRGB::Black);      break;
        case LED_MODE_RED:              _renderSolid(CRGB::Red);        break;
        case LED_MODE_BLUE:             _renderSolid(CRGB::Blue);       break;
        case LED_MODE_GREEN:            _renderSolid(CRGB::Green);      break;
        case LED_MODE_PURPLE:           _renderSolid(CRGB::Purple);     break;
        case LED_MODE_WHITE:            _renderSolid(CRGB::White);      break;
        case LED_MODE_RED_PULSE:        _renderPulse(CRGB::Red);        break;
        case LED_MODE_BLUE_PULSE:       _renderPulse(CRGB::Blue);       break;
        case LED_MODE_RED_STARTUP:      _renderStartup(CRGB::Red);      break;
        case LED_MODE_BLUE_STARTUP:     _renderStartup(CRGB::Blue);     break;
        case LED_MODE_RED_ADVANTAGE:    _renderAdvantage(CRGB::Red);    break;
        case LED_MODE_BLUE_ADVANTAGE:   _renderAdvantage(CRGB::Blue);   break;
        case LED_MODE_RAINBOW:          _renderRainbow();               break;
        case LED_MODE_SIDE_1_TEST:      _renderSideTest(1);             break;
        case LED_MODE_SIDE_2_TEST:      _renderSideTest(2);             break;
        case LED_MODE_SIDE_3_TEST:      _renderSideTest(3);             break;
        case LED_MODE_SIDE_4_TEST:      _renderSideTest(4);             break;
        default:                        _renderSolid(CRGB::Black);      break;
    }
}

void LedAnimator::_renderRainbow() {
    const int logicalPixels = NUM_SIDES * NODES_PER_FIXTURE; // 32
    const int sourcePixels = NUM_SIDES * FIXTURES_PER_SIDE * NODES_PER_FIXTURE; // 64
    uint16_t ledCount = _leds.getLedCount();

    for (uint16_t i = 0; i < ledCount; i++) {
        int sourcePixel = ((int)i * sourcePixels) / ledCount;
        int side = sourcePixel / (FIXTURES_PER_SIDE * NODES_PER_FIXTURE);
        int pixelInFixture = sourcePixel % NODES_PER_FIXTURE;
        int logicalPixel = side * NODES_PER_FIXTURE + pixelInFixture;
        int position = (logicalPixel - (_counter / 2)) % logicalPixels;
        if (position < 0) position += logicalPixels;

        _leds.setLedRaw(i, _rainbowColor((uint8_t)position));
    }
    _leds.show();
}

CRGB LedAnimator::_rainbowColor(uint8_t position) {
    const int logicalPixels = NUM_SIDES * NODES_PER_FIXTURE;
    float hue = ((float)position / (float)logicalPixels) * 6.0f;
    int segment = (int)hue;
    float fraction = hue - (float)segment;
    uint8_t q = (uint8_t)(255.0f * (1.0f - fraction));
    uint8_t t = (uint8_t)(255.0f * fraction);

    switch (segment % 6) {
        case 0: return CRGB(255, t, 0);
        case 1: return CRGB(q, 255, 0);
        case 2: return CRGB(0, 255, t);
        case 3: return CRGB(0, q, 255);
        case 4: return CRGB(t, 0, 255);
        default: return CRGB(255, 0, q);
    }
}

void LedAnimator::_renderSideTest(uint8_t side) {
    uint16_t ledCount = _leds.getLedCount();
    uint16_t start = ((uint32_t)(side - 1) * ledCount) / NUM_SIDES;
    uint16_t end = ((uint32_t)side * ledCount) / NUM_SIDES;
    CRGB color = (_role.getRole() == ROLE_RED_HUB) ? CRGB::Red : CRGB::Blue;

    for (uint16_t i = 0; i < ledCount; i++) {
        _leds.setLedRaw(i, (i >= start && i < end) ? color : CRGB::Black);
    }
    _leds.show();
}

// ─── Solid ────────────────────────────────────────────────────────────────────

void LedAnimator::_renderSolid(CRGB color) {
    _leds.showSolid(color);
}

// ─── Pulse ────────────────────────────────────────────────────────────────────

void LedAnimator::_renderPulse(CRGB color) {
    int phase = _counter % (2 * ANIM_PULSE_HALF_PERIOD);
    if (phase > ANIM_PULSE_HALF_PERIOD) {
        phase = 2 * ANIM_PULSE_HALF_PERIOD - phase;
    }
    float brightness = (float)phase / (float)ANIM_PULSE_HALF_PERIOD;
    CRGB scaled = CRGB(
        (uint8_t)(color.r * brightness),
        (uint8_t)(color.g * brightness),
        (uint8_t)(color.b * brightness)
    );
    _leds.showSolid(scaled);
}

// ─── Startup ─────────────────────────────────────────────────────────────────

void LedAnimator::_renderStartup(CRGB color) {
    // Clear all first
    uint16_t ledCount = _leds.getLedCount();
    for (int i = 0; i < ledCount; i++) {
        _leds.setLedRaw(i, CRGB::Black);
    }

    // Fill each side with its delay and direction
    int delays[]     = { STARTUP_SIDE1_DELAY, STARTUP_SIDE24_DELAY,
                         STARTUP_SIDE3_DELAY, STARTUP_SIDE24_DELAY };
    int directions[] = { FILL_CENTER_OUT, FILL_LEFT_TO_RIGHT,
                         FILL_EDGES_IN,   FILL_RIGHT_TO_LEFT };

    for (int side = 0; side < NUM_SIDES; side++) {
        _fillSide(side, color, _counter - delays[side], directions[side]);
    }

    _leds.show();
}

void LedAnimator::_fillSide(int side, CRGB color, int counter, int direction) {
    if (counter <= 0) return;

    int fillCycles  = ANIM_STARTUP_CYCLES / 3;
    float pct       = (float)counter / (float)fillCycles;
    if (pct > 1.0f) pct = 1.0f;

    for (int fixture = 0; fixture < FIXTURES_PER_SIDE; fixture++) {
        // Map source fixture to output LED range scaled to active LED count
        int srcStart = (side * FIXTURES_PER_SIDE + fixture) * NODES_PER_FIXTURE;
        int srcTotal = NUM_SIDES * FIXTURES_PER_SIDE * NODES_PER_FIXTURE; // 64
        uint16_t ledCount = _leds.getLedCount();

        // Scale source position to output strip
        int outStart = (srcStart * ledCount) / srcTotal;
        int outEnd   = ((srcStart + NODES_PER_FIXTURE) * ledCount) / srcTotal;
        int outCount = outEnd - outStart;

        _fillFixture(outStart, color, pct, direction, outCount);
    }
}

void LedAnimator::_fillFixture(int startLed, CRGB color, float percentage,
                                int direction, int count) {
    float nodesToFill = percentage * count;

    for (int rank = 0; rank < count; rank++) {
        int pixel;

        switch (direction) {
            case FILL_LEFT_TO_RIGHT:
                pixel = rank;
                break;

            case FILL_RIGHT_TO_LEFT:
                pixel = count - 1 - rank;
                break;

            case FILL_EDGES_IN:
                // Fill from both edges toward center
                if (rank % 2 == 0)
                    pixel = rank / 2;               // Left edge inward
                else
                    pixel = count - 1 - (rank / 2); // Right edge inward
                break;

            default: // FILL_CENTER_OUT
                // Fill from center outward
                int center = count / 2;
                if (rank % 2 == 0)
                    pixel = center + (rank / 2);    // Right of center
                else
                    pixel = center - 1 - (rank / 2); // Left of center
                break;
        }

        pixel = constrain(pixel, 0, count - 1);

        float brightness = nodesToFill - (float)rank;
        brightness = constrain(brightness, 0.0f, 1.0f);

        _leds.setLedRaw(startLed + pixel, CRGB(
            (uint8_t)(color.r * brightness),
            (uint8_t)(color.g * brightness),
            (uint8_t)(color.b * brightness)
        ));
    }
}

// ─── Advantage ───────────────────────────────────────────────────────────────

void LedAnimator::_renderAdvantage(CRGB color) {
    // Fill base color first
    uint16_t ledCount = _leds.getLedCount();
    for (int i = 0; i < ledCount; i++) {
        _leds.setLedRaw(i, color);
    }

    int srcTotal = NUM_SIDES * FIXTURES_PER_SIDE * NODES_PER_FIXTURE;

    for (int side = 0; side < NUM_SIDES; side++) {
        int direction = (side == 1 || side == 3) ?
                        FILL_RIGHT_TO_LEFT : FILL_LEFT_TO_RIGHT;

        for (int fixture = 0; fixture < FIXTURES_PER_SIDE; fixture++) {
            int srcStart = (side * FIXTURES_PER_SIDE + fixture) * NODES_PER_FIXTURE;
            int outStart = (srcStart * ledCount) / srcTotal;
            int outEnd   = ((srcStart + NODES_PER_FIXTURE) * ledCount) / srcTotal;
            int outCount = outEnd - outStart;

            _sweepFixture(outStart, _counter, direction, outCount);
        }
    }

    _leds.show();
}

void LedAnimator::_sweepFixture(int startLed, int counter,
                                 int direction, int count) {
    int cycleLength = count * 2 + 2;
    int position    = (counter / ANIM_ADVANTAGE_STEP_CYCLE) % cycleLength;

    if (direction == FILL_RIGHT_TO_LEFT) {
        position = cycleLength - 1 - position;
    }

    int head = position - 1;

    for (int trail = 0; trail < count; trail++) {
        int pixel = (direction == FILL_RIGHT_TO_LEFT) ?
                    head + trail - count : head - trail;

        if (pixel < 0 || pixel >= count) continue;

        float brightness = 1.0f - (float)trail / (float)count;
        _leds.setLedRaw(startLed + pixel, CRGB(
            (uint8_t)(255 * brightness),
            (uint8_t)(255 * brightness),
            (uint8_t)(255 * brightness)
        ));
    }
}
