#include "gateway/OrderGateway.h"

#include <type_traits>
#include <variant>

namespace sixchange {
    OrderGateway::OrderGateway(MatchingEngine &engine, Sequencer& sequencer) noexcept : engine_{engine}, sequencer_{sequencer} {}

    protocol::OutboundMessage OrderGateway::handle(const protocol::InboundMessage &message) {
        return std::visit(
            [this]<typename T>(const T& request) -> protocol::OutboundMessage {
                using Request = std::remove_cvref_t<T>;

                if constexpr (std::same_as<Request, protocol::NewOrderRequest>) {
                    return handle_new_order(request);
                }

                return protocol::OrderRejected{
                    .client_order_id = request.client_order_id,
                    .reason = RejectReason::InvalidMessage
                };
            },
        message);
    }

    std::optional<SymbolId> OrderGateway::resolve_symbol(std::string_view symbol) const noexcept {
        // TODO Symbol registry
        if (symbol == "AAPL") {
            return SymbolId{0};
        }

        if (symbol == "MSFT") {
            return SymbolId{1};
        }

        return std::nullopt;
    }

    OrderId OrderGateway::next_order_id() noexcept {
        return next_order_id_++;
    }

    protocol::OutboundMessage OrderGateway::handle_new_order(const protocol::NewOrderRequest& request) {
        if (client_orders_.contains(request.client_order_id)) {
            return protocol::OrderRejected{
                .client_order_id = request.client_order_id,
                .reason = RejectReason::DuplicateClientOrderId
            };
        }

        const auto symbol_id = resolve_symbol(request.symbol);

        if (!symbol_id) {
            return protocol::OrderRejected{
                .client_order_id = request.client_order_id,
                .reason = RejectReason::UnknownSymbol
            };
        }

        const auto order_id = next_order_id();

        const SequenceNumber sequence_number = sequencer_.next();

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

        engine_.process(command);

        client_orders_.emplace(request.client_order_id, order_id);

        return protocol::OrderAccepted{
            .client_order_id = request.client_order_id,
            .order_id = order_id
        };
    }
} // namespace sixchange