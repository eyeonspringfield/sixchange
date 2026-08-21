#include "MatchingEngine.h"

#include "logging/Log.h"
#include "core/EnumNames.h"

namespace sixchange {
MatchingEngine::MatchingEngine(const SymbolId symbol_id, const EngineEventSink event_sink, OrderBookConfig config)
    : event_sink_{event_sink}, order_book_(std::make_unique<OrderBook>(symbol_id, config)) {
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

void MatchingEngine::process(const EngineCommand& command) noexcept {
    switch (command.type) {
    case CommandType::NewOrder:
        process_new_order(command.new_order);
        return;

    case CommandType::CancelOrder:
        process_cancel_order(command.cancel_order);
        return;

    case CommandType::ReplaceOrder:
        // TODO replace
        emit_rejection(
            command.replace_order.seq,
            command.replace_order.old_client_order_id,
            command.replace_order.client_id,
            command.replace_order.symbol_id,
            CommandType::ReplaceOrder,
            RejectReason::MatchingEngineError
        );
        return;
    }

    SIXCHANGE_LOG_CRITICAL(
        "Invalid command type={}",
        static_cast<unsigned>(command.type)
    );
}

void MatchingEngine::process_new_order(const NewOrderCommand& command) noexcept {
    if (command.order_type != OrderType::Limit) {
        emit_rejection(
            command.seq,
            command.client_order_id,
            command.client_id,
            command.symbol_id,
            CommandType::NewOrder,
            RejectReason::UnsupportedOrderType
        );

        return;
    }

    if (command.tif != TimeInForce::GFD) {
        emit_rejection(
            command.seq,
            command.client_order_id,
            command.client_id,
            command.symbol_id,
            CommandType::NewOrder,
            RejectReason::UnsupportedTimeInForce
        );

        return;
    }

    const OrderId order_id = next_order_id_;

    SIXCHANGE_LOG_TRACE(
        "Processing new order sequence={}, assigned_order_id={}",
        command.seq,
        order_id
    );

    const auto admitted = order_book_->admit(command, order_id);

    if (!admitted) {
        emit_rejection(
            command.seq,
            command.client_order_id,
            command.client_id,
            command.symbol_id,
            CommandType::NewOrder,
            admitted.error()
        );

        return;
    }
    ++next_order_id_;

    event_sink_.emit(
        EngineEvent{
            .type = EngineEventType::OrderAccepted,
            .order_accepted = {
                .seq = command.seq,
                .order_id = order_id,
                .client_order_id = command.client_order_id,
                .client_id = command.client_id,
                .symbol_id = command.symbol_id,
                .side = command.side,
                .order_type = command.order_type,
                .tif = command.tif,
                .price = command.price,
                .quantity = command.quantity
            }
        }
    );

    const NewOrderOutcome outcome = order_book_->execute(*admitted, MatchSink::from(*this));

    if (outcome.rested) {
        event_sink_.emit(
            EngineEvent{
                .type = EngineEventType::OrderRested,
                .order_rested = {
                    .seq = command.seq,
                    .order_id = order_id,
                    .client_order_id = command.client_order_id,
                    .client_id = command.client_id,
                    .symbol_id = command.symbol_id,
                    .side = command.side,
                    .price = command.price,
                    .remaining_quantity = outcome.remaining_quantity
                }
            }
        );
    }
}

void MatchingEngine::process_cancel_order(const CancelOrderCommand& command) noexcept {
    SIXCHANGE_LOG_TRACE(
        "Processing cancel sequence={}, order_id={}",
        command.seq,
        command.order_id
    );

    const auto result = order_book_->cancel(command);

    if (!result) {
        emit_rejection(
            command.seq,
            command.client_order_id,
            command.client_id,
            command.symbol_id,
            CommandType::CancelOrder,
            result.error()
        );

        return;
    }

    const CancelledOrder& cancelled = *result;

    event_sink_.emit(
        EngineEvent{
            .type = EngineEventType::OrderCancelled,
            .order_cancelled = {
                .seq = command.seq,
                .order_id = cancelled.order_id,
                .client_order_id = cancelled.client_order_id,
                .client_id = cancelled.client_id,
                .symbol_id = cancelled.symbol_id,
                .side = cancelled.side,
                .price = cancelled.price,
                .cancelled_quantity = cancelled.cancelled_quantity
            }
        }
    );
}

void MatchingEngine::on_match(const MatchExecution& execution) noexcept {
    const TradeId trade_id = next_trade_id_++;

    event_sink_.emit(
        EngineEvent{
            .type = EngineEventType::TradeExecuted,
            .trade_executed = {
                .seq = execution.seq,
                .trade_id = trade_id,
                .symbol_id = execution.symbol_id,
                .price = execution.price,
                .quantity = execution.quantity,
                .aggressing_side = execution.aggressing_side,
                .aggressing_order_id = execution.aggressing_order_id,
                .aggressing_client_order_id = execution.aggressing_client_order_id,
                .aggressing_client_id = execution.aggressing_client_id,
                .aggressing_remaining_quantity = execution.aggressing_remaining_quantity,
                .resting_order_id = execution.resting_order_id,
                .resting_client_order_id = execution.resting_client_order_id,
                .resting_client_id = execution.resting_client_id,
                .resting_remaining_quantity = execution.resting_remaining_quantity
            }
        }
    );
}

void MatchingEngine::emit_rejection(const SequenceNumber seq,
                                    const ClientOrderId client_order_id,
                                    const ClientId client_id,
                                    const SymbolId symbol_id,
                                    const CommandType command_type,
                                    const RejectReason reason) const noexcept {
    event_sink_.emit(
        EngineEvent{
            .type = EngineEventType::CommandRejected,
            .command_rejected = {
                .seq = seq,
                .client_order_id = client_order_id,
                .client_id = client_id,
                .symbol_id = symbol_id,
                .command_type = command_type,
                .reason = reason
            }
        }
    );
}

} // namespace sixchange
