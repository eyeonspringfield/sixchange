#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <expected>
#include <span>
#include <stdexcept>

#include <sixchange/core/EngineEventSink.h>
#include <sixchange/protocol/Messages.h>

#include "engine/MatchingEngine.h"

namespace sixchange::test {

class EngineEventCollector {
public:
    static constexpr std::size_t Capacity{128};

    void on_engine_event(const EngineEvent& event) noexcept {
        assert(size_ < Capacity);
        events_[size_++] = event;
    }

    void clear() noexcept {
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] const EngineEvent& operator[](const std::size_t index) const noexcept {
        assert(index < size_);
        return events_[index];
    }

    [[nodiscard]] std::span<const EngineEvent> view() const noexcept {
        return {events_.data(), size_};
    }

private:
    std::array<EngineEvent, Capacity> events_{};
    std::size_t size_{};
};

class OutboundMessageCollector {
public:
    static constexpr std::size_t Capacity{16};

    void on_outbound_message(const protocol::OutboundMessage& message) noexcept {
        assert(size_ < Capacity);
        messages_[size_++] = message;
    }

    void clear() noexcept {
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] protocol::OutboundMessage take_single() {
        if (size_ != 1) {
            throw std::logic_error{"Expected exactly one outbound message"};
        }

        const protocol::OutboundMessage result = messages_[0];
        clear();
        return result;
    }

private:
    std::array<protocol::OutboundMessage, Capacity> messages_{};
    std::size_t size_{};
};

class MatchingEngineHarness {
public:
    explicit MatchingEngineHarness(
        const SymbolId symbol_id,
        const ::sixchange::OrderBookConfig& config = DefaultOrderBookConfig)
        : engine_{symbol_id, EngineEventSink::from(events_), config} {
    }

    [[nodiscard]] std::expected<OrderId, RejectReason> process(const EngineCommand& command) {
        events_.clear();
        engine_.process(command);

        for (const EngineEvent& event : events_.view()) {
            switch (event.type) {
            case EngineEventType::OrderAccepted:
                return event.order_accepted.order_id;

            case EngineEventType::OrderCancelled:
                return event.order_cancelled.order_id;

            case EngineEventType::CommandRejected:
                return std::unexpected{event.command_rejected.reason};

            case EngineEventType::OrderRested:
            case EngineEventType::TradeExecuted:
            case EngineEventType::OrderExpired:
            case EngineEventType::OrderReplaced:
                break;
            }
        }

        throw std::logic_error{"Engine command produced no terminal gateway-facing event"};
    }

    [[nodiscard]] const OrderBook& order_book() const noexcept {
        return engine_.order_book();
    }

    [[nodiscard]] std::uint64_t execution_count() const noexcept {
        return engine_.execution_count();
    }

    [[nodiscard]] const EngineEventCollector& events() const noexcept {
        return events_;
    }

private:
    EngineEventCollector events_{};
    MatchingEngine engine_;
};

} // namespace sixchange::test
