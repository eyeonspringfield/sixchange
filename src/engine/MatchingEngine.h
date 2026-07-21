#pragma once

#include <memory>
#include <expected>

#include <sixchange/core/Commands.h>
#include <sixchange/core/EventBuffer.h>

#include "OrderBook.h"

namespace sixchange {

class MatchingEngine {
public:
    using ProcessResult = std::expected<OrderId, RejectReason>;

    explicit MatchingEngine(SymbolId symbol_id);
    ~MatchingEngine();

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    MatchingEngine(MatchingEngine&&) noexcept;
    MatchingEngine& operator=(MatchingEngine&&) noexcept;

    [[nodiscard]] ProcessResult process(const EngineCommand& command) noexcept;

    [[nodiscard]] const OrderBook& order_book() const noexcept {
        return *order_book_;
    }

private:
    std::unique_ptr<OrderBook> order_book_;
    OrderId next_order_id_{1};
    TradeId next_trade_id_{1};
};

} // namespace sixchange
