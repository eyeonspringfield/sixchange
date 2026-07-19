#pragma once

#include <array>
#include <span>

#include <sixchange/core/Events.h>

namespace sixchange {

template <std::size_t Capacity>
class EventBuffer {
public:
    void clear() noexcept {
        size_ = 0;
    }

    bool push(const EngineEvent& event) noexcept {
        if (size_ == Capacity) {
            return false;
        }

        events_[size_++] = event;
        return true;
    }

    [[nodiscard]] std::span<const EngineEvent> view() const noexcept {
        return {events_.data(), size_};
    }

private:
    std::array<EngineEvent, Capacity> events_{};
    std::size_t size_{0};
};

} // namespace sixchange
