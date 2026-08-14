#pragma once

#include <memory>
#include <expected>

#include <sixchange/core/Commands.h>
#include <sixchange/core/EventBuffer.h>

#include "OrderBook.h"

namespace sixchange {

class MatchingEngine {
public:
    static constexpr std::size_t MaximumEventsPerCommand{16};

    using Events = EventBuffer<MaximumEventsPerCommand>;

    using ProcessResult = std::expected<OrderId, RejectReason>;

    explicit MatchingEngine(SymbolId symbol_id, OrderBookConfig config = DefaultOrderBookConfig);
    ~MatchingEngine();

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    MatchingEngine(MatchingEngine&&) noexcept;
    MatchingEngine& operator=(MatchingEngine&&) noexcept;

    [[nodiscard]] ProcessResult process(const EngineCommand& command, Events& events) noexcept;

    [[nodiscard]] const OrderBook& order_book() const noexcept {
        return *order_book_;
    }

    [[nodiscard]] std::uint64_t execution_count() const noexcept {
        return order_book_->execution_count();
    }

private:
    static void emit(Events& events, const EngineEvent& event) noexcept;

    static void emit_rejection(Events& events,
                                      SequenceNumber seq,
                                      ClientOrderId client_order_id,
                                      ClientId client_id,
                                      SymbolId symbol_id,
                                      RejectReason reason) noexcept;

    std::unique_ptr<OrderBook> order_book_;
    OrderId next_order_id_{1};
    [[maybe_unused]] TradeId next_trade_id_{1};
};

} // namespace sixchange
