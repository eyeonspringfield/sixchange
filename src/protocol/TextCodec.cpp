#include "protocol/TextCodec.h"

#include <charconv>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <expected>

namespace {

using namespace sixchange;
using namespace sixchange::protocol;

class TokenCursor {
public:
    explicit TokenCursor(const std::string_view input) noexcept : remaining_{input} {
    }

    [[nodiscard]] std::optional<std::string_view> next() noexcept {
        skip_whitespace();

        if (remaining_.empty()) {
            return std::nullopt;
        }

        const auto end = remaining_.find_first_of(" \t");

        if (end == std::string_view::npos) {
            const auto token = remaining_;
            remaining_ = {};
            return token;
        }

        const auto token = remaining_.substr(0, end);
        remaining_.remove_prefix(end);

        return token;
    }

    [[nodiscard]] bool empty() noexcept {
        skip_whitespace();
        return remaining_.empty();
    }

private:
    void skip_whitespace() noexcept {
        const auto first = remaining_.find_first_not_of(" \t");

        if (first == std::string_view::npos) {
            remaining_ = {};
            return;
        }

        remaining_.remove_prefix(first);
    }

    std::string_view remaining_;
};

template <std::unsigned_integral T>
[[nodiscard]] std::optional<T> parse_unsigned(const std::string_view token) noexcept {
    if (token.empty()) {
        return std::nullopt;
    }

    T value{};

    const char* begin = token.data();
    const char* end = begin + token.size();

    const auto [ptr, error] = std::from_chars(begin, end, value);

    if (error != std::errc{} || ptr != end) {
        return std::nullopt;
    }

    return value;
}

[[nodiscard]] std::optional<Side> parse_side(std::string_view token) noexcept {
    if (token == "B") {
        return Side::Buy;
    }

    if (token == "S") {
        return Side::Sell;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<OrderType> parse_order_type(std::string_view token) noexcept {
    if (token == "L") {
        return OrderType::Limit;
    }

    if (token == "M") {
        return OrderType::Market;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<TimeInForce> parse_time_in_force(std::string_view token) noexcept {
    if (token == "GFD") {
        return TimeInForce::GFD;
    }

    if (token == "IOC") {
        return TimeInForce::IOC;
    }

    if (token == "FOK") {
        return TimeInForce::FOK;
    }

    return std::nullopt;
}

[[nodiscard]] TextCodec::DecodeResult decode_new_order(TokenCursor& tokens) noexcept {
    const auto client_order_id_token = tokens.next();

    if (!client_order_id_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto client_order_id = parse_unsigned<ClientOrderId>(*client_order_id_token);

    if (!client_order_id || *client_order_id < 1) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto symbol_token = tokens.next();

    if (!symbol_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto side_token = tokens.next();

    if (!side_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }
    const auto side = parse_side(*side_token);

    if (!side) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto order_type_token = tokens.next();

    if (!order_type_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto order_type = parse_order_type(*order_type_token);

    if (!order_type) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto tif_token = tokens.next();

    if (!tif_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto tif = parse_time_in_force(*tif_token);

    if (!tif) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto price_token = tokens.next();

    if (!price_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto price = parse_unsigned<Price>(*price_token);

    if (!price || *price <= 0) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidPrice});
    }

    const auto quantity_token = tokens.next();

    if (!quantity_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto quantity = parse_unsigned<Quantity>(*quantity_token);

    if (!quantity || *quantity <= 0) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidQuantity});
    }

    if (!tokens.empty()) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    return InboundMessage{
        NewOrderRequest{
            .client_order_id = *client_order_id,
            .symbol = *symbol_token,
            .side = *side,
            .order_type = *order_type,
            .tif = *tif,
            .price = *price,
            .quantity = *quantity
        }
    };
}

[[nodiscard]] constexpr std::string_view reject_reason_name(const RejectReason reason) noexcept {
    switch (reason) {
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

    return "UNKNOWN_REJECT_REASON";
}

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

} // anonymous namespace

namespace sixchange::protocol {

TextCodec::DecodeResult TextCodec::decode(std::string_view message) noexcept {
    if (message.size() > max_message_length) {
        return std::unexpected<OrderRejected>({std::nullopt,RejectReason::InvalidMessage});
    }

    if (!message.empty() && message.back() == '\r') {
        message.remove_suffix(1);
    }

    if (message.empty()) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    TokenCursor tokens{message};

    const auto message_type = tokens.next();

    if (message_type == "N") {
        return decode_new_order(tokens);
    }

    return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
}

std::string TextCodec::encode(const OutboundMessage& message) {
    return std::visit(
        Overloaded{
            [](const OrderAccepted& accepted) {
                std::string output{"A "};

                output += std::to_string(
                    accepted.client_order_id
                );

                output += ' ';

                output += std::to_string(
                    accepted.order_id
                );

                return output;
            },

            [](const OrderRejected& rejected) {
                std::string output{"R "};

                if (rejected.client_order_id) {
                    output += std::to_string(
                        *rejected.client_order_id
                    );
                }
                else {
                    output += '-';
                }

                output += ' ';
                output.append(
                    reject_reason_name(rejected.reason)
                );

                return output;
            }
        },
        message
    );
}

} // namespace sixchange::protocol
