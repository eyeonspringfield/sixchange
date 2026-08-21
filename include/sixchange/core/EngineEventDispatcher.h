#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <sixchange/core/EngineEventSink.h>

namespace sixchange {

template <std::size_t Capacity = 8>
class EngineEventDispatcher {
    static_assert(Capacity > 0);

public:
    [[nodiscard]] bool add_sink(const EngineEventSink sink) noexcept {
        if (size_ == Capacity) {
            return false;
        }

        sinks_[size_++].emplace(sink);
        return true;
    }

    void on_engine_event(const EngineEvent& event) const noexcept {
        for (std::size_t index{0}; index < size_; ++index) {
            sinks_[index]->emit(event);
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

private:
    std::array<std::optional<EngineEventSink>, Capacity> sinks_{};
    std::size_t size_{};
};

} // namespace sixchange
