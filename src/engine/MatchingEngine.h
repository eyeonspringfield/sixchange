#pragma once

#include <memory>

#include <sixchange/core/Commands.h>
#include <sixchange/core/EventBuffer.h>

#include "OrderBook.h"

namespace sixchange {

    class MatchingEngine {
    public:
        explicit MatchingEngine(SymbolId symbol_id);
        ~MatchingEngine();

        MatchingEngine(const MatchingEngine &) = delete;
        MatchingEngine &operator=(const MatchingEngine &) = delete;

        MatchingEngine(MatchingEngine &&) noexcept;
        MatchingEngine &operator=(MatchingEngine &&) noexcept;

        void process(const EngineCommand &command,
                     EventBuffer<OrderBookMaxEventsPerCommand> &events) noexcept;

        [[nodiscard]] const OrderBook& order_book() const noexcept {
            return *order_book_;
        }

    private:
        std::unique_ptr<OrderBook> order_book_;
        OrderId next_order_id_{1};
        TradeId next_trade_id_{1};
    };

} // namespace sixchange
