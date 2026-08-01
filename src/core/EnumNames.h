#pragma once

#include <string_view>

#include <sixchange/core/Enums.h>

namespace sixchange::names {

[[nodiscard]] constexpr std::string_view side(const Side value) noexcept {
    switch (value) {
    case Side::Buy:
        return "BUY";

    case Side::Sell:
        return "SELL";
    }

    return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view order_type(const OrderType value) noexcept {
    switch (value) {
    case OrderType::Limit:
        return "LIMIT";

    case OrderType::Market:
        return "MARKET";
    }

    return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view time_in_force(const TimeInForce value) noexcept {
    switch (value) {
    case TimeInForce::GFD:
        return "GFD";

    case TimeInForce::IOC:
        return "IOC";

    case TimeInForce::FOK:
        return "FOK";
    }

    return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view command_type(const CommandType value) noexcept {
    switch (value) {
    case CommandType::NewOrder:
        return "NEW_ORDER";

    case CommandType::CancelOrder:
        return "CANCEL_ORDER";

    case CommandType::ReplaceOrder:
        return "REPLACE_ORDER";
    }

    return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view reject_reason(const RejectReason value) noexcept {
    switch (value) {
    case RejectReason::None:
        return "NONE";

    case RejectReason::InvalidMessage:
        return "INVALID_MESSAGE";

    case RejectReason::UnknownSymbol:
        return "UNKNOWN_SYMBOL";

    case RejectReason::InvalidPrice:
        return "INVALID_PRICE";

    case RejectReason::InvalidQuantity:
        return "INVALID_QUANTITY";

    case RejectReason::DuplicateClientOrderId:
        return "DUPLICATE_CLIENT_ORDER_ID";

    case RejectReason::UnknownOrder:
        return "UNKNOWN_ORDER";

    case RejectReason::NotOrderOwner:
        return "NOT_ORDER_OWNER";

    case RejectReason::UnsupportedOrderType:
        return "UNSUPPORTED_ORDER_TYPE";

    case RejectReason::UnsupportedTimeInForce:
        return "UNSUPPORTED_TIME_IN_FORCE";

    case RejectReason::GatewayError:
        return "GATEWAY_ERROR";

    case RejectReason::CapacityExhausted:
        return "CAPACITY_EXHAUSTED";

    case RejectReason::MatchingEngineError:
        return "MATCHING_ENGINE_ERROR";
    }

    return "UNKNOWN";
}
} // namespace sixchange::names
