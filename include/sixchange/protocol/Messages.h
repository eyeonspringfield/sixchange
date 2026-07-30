#pragma once

#include <optional>
#include <variant>
#include <string_view>

#include <sixchange/core/Types.h>
#include <sixchange/core/Enums.h>

namespace sixchange::protocol {

struct NewOrderRequest {
    ClientOrderId client_order_id{};
    std::string_view symbol;
    Side side{};
    OrderType order_type{};
    TimeInForce tif{};
    Price price{};
    Quantity quantity{};
};

struct CancelOrderRequest {
    ClientOrderId client_order_id{};
    std::string_view symbol;
};

struct OrderAccepted {
    ClientOrderId client_order_id{};
    OrderId order_id{};
};

struct OrderCancelled {
    ClientOrderId client_order_id{};
    OrderId order_id{};
};

struct OrderRejected {
    std::optional<ClientOrderId> client_order_id;
    RejectReason reason{};
};

using InboundMessage = std::variant<NewOrderRequest, CancelOrderRequest>;

using OutboundMessage = std::variant<OrderAccepted, OrderCancelled, OrderRejected>;

} // namespace sixchange::protocol
