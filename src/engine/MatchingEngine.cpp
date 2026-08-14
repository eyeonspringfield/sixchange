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

MatchingEngine::ProcessResult MatchingEngine::process(const EngineCommand& command, Events& events) noexcept {
    events.clear();

    switch (command.type) {
    case CommandType::NewOrder: {
        const OrderId order_id = next_order_id_;
        const auto& order = command.new_order;

        SIXCHANGE_LOG_TRACE(
            "Processing new order sequence={}, assigned_order_id={}",
            command.new_order.seq,
            order_id
        );

        if (const auto result = order_book_->add(command.new_order, order_id); !result) {
            emit_rejection(events, order.seq, order.client_order_id, order.client_id, order.symbol_id, result.error());

            return std::unexpected{result.error()};
        }

        emit(
            events,
            EngineEvent{
                .seq = order.seq,
                .order_id = order_id,
                .client_order_id =
                    order.client_order_id,
                .client_id = order.client_id,
                .symbol_id = order.symbol_id,
                .type = EventType::OrderAccepted
            }
        );

        emit(
                events,
                EngineEvent{
                    .seq = order.seq,
                    .order_id = order_id,
                    .price = order.price,
                    .quantity = order.quantity,
                    .remaining_quantity =
                        order.quantity,
                    .client_order_id =
                        order.client_order_id,
                    .client_id = order.client_id,
                    .symbol_id = order.symbol_id,
                    .side = order.side,
                    .type = EventType::OrderAdded
                }
            );

        ++next_order_id_;
        return order_id;
    }

    case CommandType::CancelOrder: {
        const auto& cancel = command.cancel_order;

        SIXCHANGE_LOG_TRACE(
            "Processing cancel sequence={}, order_id={}",
            command.cancel_order.seq,
            command.cancel_order.order_id
        );

        if (const auto result = order_book_->cancel(command.cancel_order); !result) {
            emit_rejection(events, cancel.seq, cancel.client_order_id, cancel.client_id, cancel.symbol_id, result.error());

            return std::unexpected{result.error()};
        }

        emit(
        events,
        EngineEvent{
            .seq = cancel.seq,
            .order_id = cancel.order_id,
            .client_order_id =
                cancel.client_order_id,
            .client_id = cancel.client_id,
            .symbol_id = cancel.symbol_id,
            .type =
                EventType::OrderCancelled
        }
    );

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

void MatchingEngine::emit(Events& events, const EngineEvent& event) noexcept {
    if (events.push(event)) {
        return;
    }

    SIXCHANGE_LOG_CRITICAL("Engine event buffer exhausted");
}

void MatchingEngine::emit_rejection(Events& events,
                                    const SequenceNumber seq,
                                    const ClientOrderId client_order_id,
                                    const ClientId client_id,
                                    const SymbolId symbol_id,
                                    const RejectReason reason) noexcept {
    return emit(
        events,
        EngineEvent{
        .seq = seq,
        .client_order_id = client_order_id,
        .client_id = client_id,
        .symbol_id = symbol_id,
        .reject_reason = reason,
        .type = EventType::OrderRejected
    });
}

} // namespace sixchange
