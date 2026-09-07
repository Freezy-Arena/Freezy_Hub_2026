#pragma once
#include <stdint.h>

enum class InputReply { Idle, Pending, Success, Failed };

// The arena sends one untagged ACK per setInput. Never retry on a connection
// with an ambiguous response: close it first, then consume the failed result.
class InputAck {
public:
    static constexpr uint32_t TimeoutMs = 500;
    static constexpr bool valid(bool successIsBool, bool success, bool countIsInt, int count) {
        return successIsBool && success && countIsInt && count == 1;
    }
    constexpr bool begin(uint32_t now) {
        if (state != InputReply::Idle || resetRequired) return false;
        sentAt = now;
        state = InputReply::Pending;
        return true;
    }
    constexpr void reply(bool valid) {
        if (state != InputReply::Pending) return;
        state = valid ? InputReply::Success : InputReply::Failed;
        if (!valid) resetRequired = true;
    }
    constexpr void expire(uint32_t now) {
        if (state == InputReply::Pending && uint32_t(now - sentAt) >= TimeoutMs) reply(false);
    }
    constexpr void disconnected() {
        if (state == InputReply::Pending) state = InputReply::Failed;
        resetRequired = false; // Old TCP stream is gone; its ACKs cannot be reused.
    }
    constexpr InputReply take() {
        InputReply result = state;
        if (!resetRequired && (state == InputReply::Success || state == InputReply::Failed)) state = InputReply::Idle;
        return result;
    }
    InputReply state = InputReply::Idle;
    bool resetRequired = false;
private:
    uint32_t sentAt = 0;
};
