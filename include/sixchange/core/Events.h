#pragma once

#include <type_traits>

#include <sixchange/core/Enums.h>
#include <sixchange/core/Types.h>

namespace sixchange {

enum class EngineEventType : std::uint8_t {
    OrderAccepted,
    CommandRejected,
    OrderRested,
    TradeExecuted,
    OrderCancelled,
    OrderExpired,
    OrderReplaced
};

struct OrderAcceptedEvent {
    SequenceNumber seq{};

    OrderId order_id{};
    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Side side{};
    OrderType order_type{};
    TimeInForce tif{};

    Price price{};
    Quantity quantity{};
};

struct CommandRejectedEvent {
    SequenceNumber seq{};

    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    CommandType command_type{};
    RejectReason reason{};
};

struct OrderRestedEvent {
    SequenceNumber seq{};

    OrderId order_id{};
    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Side side{};
    Price price{};
    Quantity remaining_quantity{};
};

struct TradeExecutedEvent {
    SequenceNumber seq{};
    TradeId trade_id{};

    SymbolId symbol_id{};

    Price price{};
    Quantity quantity{};

    Side aggressing_side{};

    OrderId aggressing_order_id{};
    ClientOrderId aggressing_client_order_id{};
    ClientId aggressing_client_id{};
    Quantity aggressing_remaining_quantity{};

    OrderId resting_order_id{};
    ClientOrderId resting_client_order_id{};
    ClientId resting_client_id{};
    Quantity resting_remaining_quantity{};
};

struct OrderCancelledEvent {
    SequenceNumber seq{};

    OrderId order_id{};
    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Side side{};
    Price price{};
    Quantity cancelled_quantity{};
};

struct OrderExpiredEvent {
    SequenceNumber seq{};

    OrderId order_id{};
    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Quantity expired_quantity{};
};

struct OrderAmendedEvent {
    SequenceNumber seq{};

    OrderId order_id{};
    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Price price{};

    Quantity old_quantity{};
    Quantity new_quantity{};
};

struct OrderReplacedEvent {
    SequenceNumber seq{};

    OrderId order_id{};
    ClientOrderId old_client_order_id{};
    ClientOrderId new_client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Price new_price{};
    Quantity new_quantity{};
};

struct EngineEvent {
    EngineEventType type{};

    union {
        OrderAcceptedEvent order_accepted;
        CommandRejectedEvent command_rejected;
        OrderRestedEvent order_rested;
        TradeExecutedEvent trade_executed;
        OrderCancelledEvent order_cancelled;
        OrderExpiredEvent order_expired;
        OrderReplacedEvent order_replaced;
    };
};

static_assert(std::is_trivially_copyable_v<EngineEvent>);

} // namespace sixchange