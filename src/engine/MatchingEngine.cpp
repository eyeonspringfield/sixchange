#include "MatchingEngine.h"

#include "logging/Log.h"
#include "core/EnumNames.h"

namespace sixchange {
MatchingEngine::MatchingEngine(const SymbolId symbol_id, OrderBookConfig config)
    : order_book_(std::make_unique<OrderBook>(symbol_id, config)) {
    SIXCHANGE_LOG_INFO(
        "Matching engine initialized symbol_id={}, max_active_orders={}, price_level_count={}, order_lookup_capacity={}",
        symbol_id,
        config.max_active_orders,
        config.price_level_count,
        config.order_lookup_capacity
    );
}

MatchingEngine::~MatchingEngine() = default;

MatchingEngine::MatchingEngine(MatchingEngine&&) noexcept = default;

MatchingEngine& MatchingEngine::operator=(MatchingEngine&&) noexcept = default;

MatchingEngine::ProcessResult MatchingEngine::process(const EngineCommand& command) noexcept {
    switch (command.type) {
    case CommandType::NewOrder: {
        const OrderId order_id = next_order_id_;

        SIXCHANGE_LOG_TRACE(
            "Processing new order sequence={}, assigned_order_id={}",
            command.new_order.seq,
            order_id
        );

        if (const auto result = order_book_->add(command.new_order, order_id); !result) {
            return std::unexpected{result.error()};
        }

        ++next_order_id_;
        return order_id;
    }

    case CommandType::CancelOrder: {
        SIXCHANGE_LOG_TRACE(
            "Processing cancel sequence={}, order_id={}",
            command.cancel_order.seq,
            command.cancel_order.order_id
        );

        if (const auto result = order_book_->cancel(command.cancel_order); !result) {
            return std::unexpected{result.error()};
        }

        return command.cancel_order.order_id;
    }

    default:
        SIXCHANGE_LOG_ERROR(
            "Unsupported command type={}",
            names::command_type(command.type)
        );

        return std::unexpected{RejectReason::MatchingEngineError};
    }
}
} // namespace sixchange
