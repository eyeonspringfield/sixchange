#pragma once

#include <memory>
#include <expected>

#include <sixchange/core/Commands.h>
#include <sixchange/core/EngineEventSink.h>

#include "OrderBook.h"

namespace sixchange {

class MatchingEngine {
public:
    MatchingEngine(SymbolId symbol_id, EngineEventSink event_sink, OrderBookConfig config = DefaultOrderBookConfig);
    ~MatchingEngine();

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    MatchingEngine(MatchingEngine&&) noexcept;
    MatchingEngine& operator=(MatchingEngine&&) noexcept;

    void process(const EngineCommand& command) noexcept;

    [[nodiscard]] const OrderBook& order_book() const noexcept {
        return *order_book_;
    }

    [[nodiscard]] std::uint64_t execution_count() const noexcept {
        return order_book_->execution_count();
    }

    void on_match(const MatchExecution& match_execution) noexcept;

private:
    void process_new_order(const NewOrderCommand& command) noexcept;

    void process_cancel_order(const CancelOrderCommand& command) noexcept;

    void emit_rejection(SequenceNumber seq,
                        ClientOrderId client_order_id,
                        ClientId client_id,
                        SymbolId symbol_id,
                        CommandType command_type,
                        RejectReason reason) const noexcept;

    EngineEventSink event_sink_;

    std::unique_ptr<OrderBook> order_book_;

    OrderId next_order_id_{1};
    [[maybe_unused]] TradeId next_trade_id_{1};
};

} // namespace sixchange
