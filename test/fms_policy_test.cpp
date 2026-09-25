// Compile-time behavioral tests: run with any C++14 compiler, including the
// installed ESP32 toolchain (-std=c++14 -fsyntax-only). No hardware required.
#include "../src/fms_table/stop_policy.h"
#include "../src/websocket/input_ack.h"
using namespace fms;

constexpr bool rapidTransitionsAndOutage() {
    StopHistory<256> history;
    history.observe(true, 0); // Boot snapshot.
    for (uint32_t i = 1; i <= 200; ++i) history.observe(i % 2 == 0, i * 2);
    if (history.count != 201 || history.fault) return false;
    Transition item;
    for (uint32_t i = 0; i <= 200; ++i) {
        if (!history.peek(item) || item.sequence != i + 1 || item.state != (i % 2 == 0)) return false;
        // Failed/absent responses leave the same head, including across retries.
        Transition retry;
        if (!history.peek(retry) || retry.sequence != item.sequence) return false;
        if (history.acknowledge(item.sequence + 1)) return false;
        if (!history.acknowledge(item.sequence)) return false;
    }
    return history.count == 0 && history.highWater == 201;
}
constexpr bool queueWrapAndOverflow() {
    StopHistory<3> history;
    Transition item;
    for (int i = 0; i < 12; ++i) {
        history.observe(i % 2, i);
        if (!history.peek(item) || !history.acknowledge(item.sequence)) return false;
    }
    history.observe(false, 20);
    history.observe(true, 21);
    history.observe(false, 22);
    history.observe(true, 23); // Full: retain history and latch, never overwrite.
    history.observe(false, 24);
    history.observe(false, 25); // Same sample is not a new lost transition.
    if (!history.peek(item)) return false;
    return history.fault && history.count == 3 && history.unretained == 2
        && item.sequence == 13 && !history.acknowledge(item.sequence) && !history.latest;
}
constexpr bool heldAndBouncingStart() {
    StartEdge button;
    if (button.sample(true, 0) || button.sample(true, 5000)) return false; // Held at boot.
    if (button.sample(false, 5001) || button.sample(false, 5031)) return false;
    if (button.sample(true, 5040) || button.sample(false, 5045)) return false;
    if (button.sample(true, 5050) || button.sample(true, 5079)) return false;
    if (!button.sample(true, 5080) || button.sample(true, 20000)) return false;
    // Brief release bounce cannot re-arm.
    if (button.sample(false, 20001) || button.sample(true, 20010) || button.sample(true, 20040)) return false;
    if (button.sample(false, 20050) || button.sample(false, 20080)) return false;
    return !button.sample(true, 20090) && button.sample(true, 20120);
}
constexpr bool rolloverAndSteadyInput() {
    StartEdge button;
    button.sample(false, 0xffffffe0u);
    button.sample(false, 0xfffffffeu);
    button.sample(true, 0xffffffffu);
    if (!button.sample(true, 29u)) return false;
    StopHistory<2> history;
    for (int i = 0; i < 1000; ++i) history.observe(true, i);
    return history.count == 1 && history.observed == 1 && !history.fault;
}
static_assert(StopPin == 33 && StartPin == 34, "Legacy table wiring");
static_assert(StopChannel == INPUT_FIELD_ESTOP, "Table stop uses the PLC input map");
static_assert(!wireState(true) && wireState(false), "HIGH is field stop / wire false");
static_assert(rapidTransitionsAndOutage(), "Ordered transitions retained through outage/retries");
static_assert(queueWrapAndOverflow(), "Overflow latches and freezes retained history");
static_assert(heldAndBouncingStart(), "One start per debounced release/press");
static_assert(rolloverAndSteadyInput(), "Clock rollover and no duplicate steady samples");
constexpr bool websocketAcknowledgments() {
    InputAck ack;
    ack.reply(true); // Unsolicited ACK cannot change idle state.
    if (ack.take() != InputReply::Idle || !ack.begin(100)) return false;
    if (ack.begin(101) || ack.take() != InputReply::Pending) return false;
    ack.expire(599);
    if (ack.take() != InputReply::Pending) return false;
    ack.expire(600);
    ack.reply(true); // A late ACK after timeout cannot turn failure into success.
    if (!ack.resetRequired || ack.take() != InputReply::Failed || ack.begin(601)) return false;
    ack.disconnected();
    if (ack.take() != InputReply::Failed || !ack.begin(700)) return false;
    ack.reply(true);
    if (ack.take() != InputReply::Success || ack.take() != InputReply::Idle) return false;
    if (!ack.begin(800)) return false;
    ack.reply(false); // Invalid count/success/error requires a fresh connection.
    if (!ack.resetRequired || ack.begin(801)) return false;
    ack.disconnected();
    if (ack.take() != InputReply::Failed || !ack.begin(900)) return false;
    ack.disconnected(); // Disconnect before ACK retains head in delivery policy.
    if (ack.take() != InputReply::Failed || !ack.begin(0xfffffff0u)) return false;
    ack.expire(484u); // Timeout across millis rollover.
    return ack.resetRequired && ack.take() == InputReply::Failed;
}
static_assert(websocketAcknowledgments(), "One request in flight, validated ACK, reset before ambiguous retry");
static_assert(InputAck::valid(true, true, true, 1)
    && !InputAck::valid(false, true, true, 1)
    && !InputAck::valid(true, false, true, 1)
    && !InputAck::valid(true, true, false, 1)
    && !InputAck::valid(true, true, true, 0)
    && !InputAck::valid(true, true, true, 2), "Only typed success=true and count=1 acknowledge an input");
static_assert(retryDelay(1) == 50 && retryDelay(2) == 100 && retryDelay(3) == 200
    && retryDelay(4) == 400 && retryDelay(5) == 1000 && retryDelay(255) == 1000, "Bounded backoff");
static_assert(startExpired(100, 100, false) && startExpired(601, 100, true)
    && !startExpired(600, 100, true) && !startExpired(10, 0xfffffff0u, true),
    "Offline/expired starts are canceled, including around clock rollover");
static_assert(releaseHeld(true, 199, 100) && !releaseHeld(true, 200, 100)
    && !releaseHeld(false, 100, 100) && releaseHeld(true, 10, 0xfffffff0u),
    "Acknowledged stop remains visible for 100 ms before release");
static_assert(mayStart(true, false, 0, true, 0, false)
    && !mayStart(true, true, 0, true, 0, false)
    && !mayStart(true, false, 1, true, 0, false)
    && !mayStart(true, false, 0, false, 0, false)
    && !mayStart(true, false, 0, true, 1, false)
    && !mayStart(true, false, 0, true, 0, true)
    && !mayStart(false, false, 0, true, 0, false),
    "Fault, stop history, asserted stop, failed delivery and hold all inhibit starts");
