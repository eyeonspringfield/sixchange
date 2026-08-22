#pragma once

#include <cassert>
#include <concepts>

#include <sixchange/protocol/Messages.h>

namespace sixchange::protocol {

template <typename T>
concept OutboundMessageHandler =
    std::convertible_to<T*, void*> &&
        requires(T& target, const OutboundMessage& message) {
    { target.on_outbound_message(message) } noexcept -> std::same_as<void>;
        };

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

    template <typename T> requires OutboundMessageHandler<T>
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
