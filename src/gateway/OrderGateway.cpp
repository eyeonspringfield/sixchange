#include "gateway/OrderGateway.h"

#include <exception>
#include <type_traits>
#include <variant>

#include "core/EnumNames.h"
#include "logging/Log.h"

namespace sixchange {

OrderGateway::OrderGateway(MatchingEngine& engine, const protocol::OutboundMessageSink outbound_sink) noexcept
    : engine_{engine},
      outbound_sink_{outbound_sink} {
}

void OrderGateway::handle(const protocol::InboundMessage& message) noexcept {
    std::visit(
        [this]<typename T>(const T& request) noexcept {
            using Request = std::remove_cvref_t<T>;

            if constexpr (std::same_as<Request, protocol::NewOrderRequest>) {
                handle_new_order(request);
            } else if constexpr (std::same_as<Request, protocol::CancelOrderRequest>) {
                handle_cancel_order(request);
            }
        },
        message
    );
}

std::optional<SymbolId> OrderGateway::resolve_symbol(const std::string_view symbol) noexcept {
    // TODO Symbol registry
    if (symbol == "AAPL") {
        return SymbolId{0};
    }

    return std::nullopt;
}

void OrderGateway::handle_new_order(const protocol::NewOrderRequest& request) noexcept {
    if (client_orders_.contains(request.client_order_id)) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected client_order_id={} reason={}",
            request.client_order_id,
            names::reject_reason(RejectReason::DuplicateClientOrderId)
        );

        emit_rejection(request.client_order_id, RejectReason::DuplicateClientOrderId);
        return;
    }

    const auto symbol_id = resolve_symbol(request.symbol);

    if (!symbol_id) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected client_order_id={}, symbol={}, reason={}",
            request.client_order_id,
            request.symbol,
            names::reject_reason(RejectReason::UnknownSymbol)
        );

        emit_rejection(request.client_order_id, RejectReason::UnknownSymbol);
        return;
    }

    const SequenceNumber sequence_number = Sequencer::instance().next();

    const NewOrderCommand new_order_command{
        .seq = sequence_number,
        .price = request.price,
        .quantity = request.quantity,
        .client_order_id = request.client_order_id,
        .client_id = client_id_,
        .symbol_id = *symbol_id,
        .side = request.side,
        .order_type = request.order_type,
        .tif = request.tif
    };

    const EngineCommand command{
        .type = CommandType::NewOrder,
        .new_order = new_order_command
    };

    SIXCHANGE_LOG_TRACE(
        "Submitting new order sequence={}, client_order_id={}, symbol_id={}",
        sequence_number,
        request.client_order_id,
        *symbol_id
    );

    engine_.process(command);
}

void OrderGateway::handle_cancel_order(const protocol::CancelOrderRequest& request) noexcept {
    const auto symbol_id = resolve_symbol(request.symbol);

    if (!symbol_id) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected client_order_id={}, symbol={}, reason={}",
            request.client_order_id,
            request.symbol,
            names::reject_reason(RejectReason::UnknownSymbol)
        );

        emit_rejection(request.client_order_id, RejectReason::UnknownSymbol);
        return;
    }

    const auto iterator = client_orders_.find(request.client_order_id);

    if (iterator == client_orders_.end()) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected client_order_id={} reason={}",
            request.client_order_id,
            names::reject_reason(RejectReason::UnknownOrder)
        );

        emit_rejection(request.client_order_id, RejectReason::UnknownOrder);
        return;
    }

    const SequenceNumber sequence_number = Sequencer::instance().next();

    const CancelOrderCommand cancel{
        .seq = sequence_number,
        .order_id = iterator->second,
        .client_order_id = request.client_order_id,
        .client_id = client_id_,
        .symbol_id = *symbol_id
    };

    const EngineCommand command{
        .type = CommandType::CancelOrder,
        .cancel_order = cancel
    };

    SIXCHANGE_LOG_TRACE(
        "Submitting cancel sequence={}, client_order_id={}, order_id={}, symbol_id={}",
        sequence_number,
        request.client_order_id,
        iterator->second,
        *symbol_id
    );

    engine_.process(command);
}

void OrderGateway::on_engine_event(const EngineEvent& event) noexcept {
    switch (event.type) {
    case EngineEventType::OrderAccepted: {
        const auto& accepted = event.order_accepted;

        if (accepted.client_id != client_id_) {
            return;
        }

        const auto [iterator, inserted] = client_orders_.emplace(
            accepted.client_order_id,
            accepted.order_id
        );
        (void)iterator;

        if (!inserted) {
            SIXCHANGE_LOG_CRITICAL(
                "Accepted order already exists in gateway mapping client_order_id={}, order_id={}",
                accepted.client_order_id,
                accepted.order_id
            );
            std::terminate();
        }

        SIXCHANGE_LOG_DEBUG(
            "New order accepted sequence={}, client_order_id={}, order_id={}, symbol_id={}",
            accepted.seq,
            accepted.client_order_id,
            accepted.order_id,
            accepted.symbol_id
        );

        outbound_sink_.emit(
            protocol::OutboundMessage{
                protocol::OrderAccepted{
                    .client_order_id = accepted.client_order_id,
                    .order_id = accepted.order_id
                }
            }
        );
        return;
    }

    case EngineEventType::CommandRejected: {
        const auto& rejected = event.command_rejected;

        if (rejected.client_id != client_id_) {
            return;
        }

        SIXCHANGE_LOG_DEBUG(
            "Command rejected by engine sequence={}, client_order_id={}, command_type={}, reason={}",
            rejected.seq,
            rejected.client_order_id,
            names::command_type(rejected.command_type),
            names::reject_reason(rejected.reason)
        );

        emit_rejection(rejected.client_order_id, rejected.reason);
        return;
    }

    case EngineEventType::OrderCancelled: {
        const auto& cancelled = event.order_cancelled;

        if (cancelled.client_id != client_id_) {
            return;
        }

        SIXCHANGE_LOG_DEBUG(
            "Cancel accepted sequence={}, client_order_id={}, order_id={}",
            cancelled.seq,
            cancelled.client_order_id,
            cancelled.order_id
        );

        outbound_sink_.emit(
            protocol::OutboundMessage{
                protocol::OrderCancelled{
                    .client_order_id = cancelled.client_order_id,
                    .order_id = cancelled.order_id
                }
            }
        );
        return;
    }

    case EngineEventType::OrderRested:
    case EngineEventType::TradeExecuted:
    case EngineEventType::OrderExpired:
    case EngineEventType::OrderReplaced:
        return;
    }
}

void OrderGateway::emit_rejection(
    const std::optional<ClientOrderId> client_order_id,
    const RejectReason reason) const noexcept {
    outbound_sink_.emit(
        protocol::OutboundMessage{
            protocol::OrderRejected{
                .client_order_id = client_order_id,
                .reason = reason
            }
        }
    );
}

} // namespace sixchange
