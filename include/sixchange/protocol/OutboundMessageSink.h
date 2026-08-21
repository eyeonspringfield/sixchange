#pragma once

#include <cassert>

#include <sixchange/protocol/Messages.h>

namespace sixchange::protocol {

class OutboundMessageSink {
public:
    using Callback = void (*)(void*, const OutboundMessage&) noexcept;

    constexpr OutboundMessageSink(void* context, const Callback callback) noexcept
        : context_{context}, callback_{callback} {
        assert(callback_ != nullptr);
    }

    void emit(const OutboundMessage& message) const noexcept {
        callback_(context_, message);
    }

    template <typename T>
    [[nodiscard]] static OutboundMessageSink from(T& target) noexcept {
        return OutboundMessageSink{
            &target,
            [](void* context, const OutboundMessage& message) noexcept {
                static_cast<T*>(context)->on_outbound_message(message);
            }
        };
    }

private:
    void* context_{};
    Callback callback_{};
};

} // namespace sixchange::protocol
