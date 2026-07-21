#pragma once

#include <cstdint>

namespace sixchange {

enum class Side : std::uint8_t {
    Buy,
    Sell
};

enum class OrderType : std::uint8_t {
    Limit,
    Market
};

enum class TimeInForce : std::uint8_t {
    GFD,
    IOC,
    FOK
};

enum class CommandType : std::uint8_t {
    NewOrder,
    CancelOrder,
    ReplaceOrder
};

enum class EventType : std::uint8_t {
    OrderAccepted,
    OrderRejected,
    OrderAdded,
    OrderExecuted,
    OrderPartiallyFilled,
    OrderFilled,
    OrderCancelled,
    OrderReplaced,
    OrderExpired
};

enum class RejectReason : std::uint8_t {
    None,
    InvalidMessage,
    UnknownSymbol,
    InvalidPrice,
    InvalidQuantity,
    DuplicateClientOrderId,
    UnknownOrder,
    NotOrderOwner,
    UnsupportedOrderType,
    UnsupportedTimeInForce,
    GatewayError,
    CapacityExhausted,
    MatchingEngineError
};

enum class LiquidityFlag : std::uint8_t {
    None,
    Maker,
    Taker
};

} // namespace sixchange
