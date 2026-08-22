#pragma once

#include <cassert>
#include <concepts>

#include "MatchExecution.h"

namespace sixchange {

template <typename T>
concept MatchHandler =
    std::convertible_to<T*, void*> &&
        requires(T& target, const MatchExecution& execution) {
            { target.on_match(execution) } noexcept -> std::same_as<void>;
        };

class MatchSink {
public:
    using Callback = void (*)(void*, const MatchExecution&) noexcept;

    constexpr MatchSink(void* context, Callback callback) noexcept : context_{context}, callback_{callback} {
        assert(callback_ != nullptr);
    }

    void emit(const MatchExecution& execution) const noexcept {
        callback_(context_, execution);
    }

    template <typename T> requires MatchHandler<T>
    [[nodiscard]] static MatchSink from(T& target) noexcept {
        return MatchSink{
            &target,
            [](void* context,
               const MatchExecution& execution) noexcept {
                static_cast<T*>(context)->on_match(execution);
            }
        };
    }

private:
    void* context_{};
    Callback callback_{};
};

} // namespace sixchange