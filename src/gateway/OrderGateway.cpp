#include "gateway/OrderGateway.h"

#include <type_traits>
#include <variant>

#include "logging/Log.h"
#include "core/EnumNames.h"

namespace sixchange {

OrderGateway::OrderGateway(MatchingEngine& engine) noexcept : engine_{engine} {
}

protocol::OutboundMessage OrderGateway::handle(const protocol::InboundMessage& message) {
    return std::visit(
        [this]<typename T>(const T& request) -> protocol::OutboundMessage {
            using Request = std::remove_cvref_t<T>;

            if constexpr (std::same_as<Request, protocol::NewOrderRequest>) {
                return handle_new_order(request);
            } else if constexpr (std::same_as<Request, protocol::CancelOrderRequest>) {
                return handle_cancel_order(request);
            }

            return protocol::OrderRejected{
                .client_order_id = request.client_order_id,
                .reason = RejectReason::GatewayError
            };
        },
        message);
}

std::optional<SymbolId> OrderGateway::resolve_symbol(const std::string_view symbol) noexcept {
    // TODO Symbol registry
    if (symbol == "AAPL") {
        return SymbolId{0};
    }

    return std::nullopt;
}

protocol::OutboundMessage OrderGateway::handle_new_order(const protocol::NewOrderRequest& request) {
    if (client_orders_.contains(request.client_order_id)) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected client_order_id={} reason={}",
            request.client_order_id,
            names::reject_reason(RejectReason::DuplicateClientOrderId)
       );

        return protocol::OrderRejected{
            .client_order_id = request.client_order_id,
            .reason = RejectReason::DuplicateClientOrderId
        };
    }

    const auto symbol_id = resolve_symbol(request.symbol);

    if (!symbol_id) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected client_order_id={}, symbol={}, reason={}",
            request.client_order_id,
            request.symbol,
            names::reject_reason(RejectReason::UnknownSymbol)
        );

        return protocol::OrderRejected{
            .client_order_id = request.client_order_id,
            .reason = RejectReason::UnknownSymbol
        };
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

    const auto result = engine_.process(command);

    if (!result) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected by engine sequence={}, client_order_id={}, reason={}",
            sequence_number,
            request.client_order_id,
            names::reject_reason(result.error())
        );

        return protocol::OrderRejected{
            .client_order_id = request.client_order_id,
            .reason = result.error()
        };
    }

    const OrderId order_id = *result;

    client_orders_.emplace(request.client_order_id, order_id);

    SIXCHANGE_LOG_DEBUG(
        "New order accepted sequence={}, client_order_id={}, order_id={}, symbol_id={}",
        sequence_number,
        request.client_order_id,
        order_id,
        *symbol_id
    );

    return protocol::OrderAccepted{
        .client_order_id = request.client_order_id,
        .order_id = order_id
    };
}

protocol::OutboundMessage OrderGateway::handle_cancel_order(const protocol::CancelOrderRequest& request) {
    const auto symbol_id = resolve_symbol(request.symbol);

    if (!symbol_id) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected client_order_id={}, symbol={}, reason={}",
            request.client_order_id,
            request.symbol,
            names::reject_reason(RejectReason::UnknownSymbol)
        );

        return protocol::OrderRejected{
            .client_order_id = request.client_order_id,
            .reason = RejectReason::UnknownSymbol
        };
    }

    const auto iterator = client_orders_.find(request.client_order_id);

    if (iterator == client_orders_.end()) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected client_order_id={} reason={}",
            request.client_order_id,
            names::reject_reason(RejectReason::UnknownOrder)
        );

        return protocol::OrderRejected{
            .client_order_id = request.client_order_id,
            .reason = RejectReason::UnknownOrder
        };
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

    const auto result = engine_.process(command);

    if (!result) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected by engine sequence={}, client_order_id={}, order_id={}, reason={}",
            sequence_number,
            request.client_order_id,
            iterator->second,
            names::reject_reason(result.error())
        );

        return protocol::OrderRejected{
            .client_order_id = request.client_order_id,
            .reason = result.error()
        };
    }

    SIXCHANGE_LOG_DEBUG(
        "Cancel accepted sequence={}, client_order_id={}, order_id={}",
        sequence_number,
        request.client_order_id,
        *result
    );

    return protocol::OrderCancelled{
        .client_order_id = request.client_order_id,
        .order_id = *result
    };
}

} // namespace sixchange
