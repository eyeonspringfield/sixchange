#pragma once

#include <cassert>
#include <concepts>

#include <sixchange/core/Events.h>

namespace sixchange {

template <typename T>
concept EngineEventHandler =
    std::convertible_to<T*, void*> &&
        requires(T& target, const EngineEvent& event) {
        { target.on_engine_event(event) } noexcept -> std::same_as<void>;
    };

class EngineEventSink {
public:
    using Callback = void (*)(void*, const EngineEvent&) noexcept;

    constexpr EngineEventSink(void* context, const Callback callback) noexcept : context_{context}, callback_{callback} {
        assert(callback_ != nullptr);
    }

    void emit(const EngineEvent& event) const noexcept {
        callback_(context_, event);
    }

    template <typename T> requires EngineEventHandler<T>
    [[nodiscard]] static EngineEventSink from(T& target) noexcept {
        return EngineEventSink{
            &target,
            [](void* context, const EngineEvent& event) noexcept {
                static_cast<T*>(context)->on_engine_event(event);
            }
        };
    }

private:
    void* context_{};
    Callback callback_{};
};

} // namespace sixchange