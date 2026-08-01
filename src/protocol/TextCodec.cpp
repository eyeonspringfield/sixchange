#include "protocol/TextCodec.h"

#include <charconv>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <expected>

#include "logging/Log.h"
#include "core/EnumNames.h"

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

[[nodiscard]] TextCodec::DecodeResult decode_cancel_order(TokenCursor& tokens) noexcept {
    const auto client_order_id_token = tokens.next();

    if (!client_order_id_token) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto client_order_id = parse_unsigned<ClientOrderId>(*client_order_id_token);

    if (!client_order_id || *client_order_id == ClientOrderId{0}) {
        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    const auto symbol = tokens.next();

    if (!symbol || !tokens.empty()) {
        return std::unexpected<OrderRejected>({client_order_id, RejectReason::InvalidMessage});
    }

    return InboundMessage{
        CancelOrderRequest{
            .client_order_id = *client_order_id,
            .symbol = *symbol
        }
    };
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
        SIXCHANGE_LOG_DEBUG(
            "Message rejected reason={}, detail={}, length={}",
            names::reject_reason(RejectReason::InvalidMessage),
            "message_too_long",
            message.size()
        );

        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    if (!message.empty() && message.back() == '\r') {
        message.remove_suffix(1);
    }

    if (message.empty()) {
        SIXCHANGE_LOG_DEBUG(
            "Message rejected reason={}, detail={}",
            names::reject_reason(RejectReason::InvalidMessage),
            "empty_message"
        );

        return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }

    TokenCursor tokens{message};

    const auto message_type = tokens.next();

    DecodeResult result =
        [&]() -> DecodeResult {
            if (message_type == "N") {
                return decode_new_order(tokens);
            }

            if (message_type == "C") {
                return decode_cancel_order(tokens);
            }

            return std::unexpected<OrderRejected>({std::nullopt, RejectReason::InvalidMessage});
    }();

    if (!result) {
        const OrderRejected& rejection = result.error();

        if (rejection.client_order_id) {
            SIXCHANGE_LOG_DEBUG(
                "Message rejected client_order_id={} reason={}",
                *rejection.client_order_id,
                names::reject_reason(rejection.reason)
            );
        }
        else {
            SIXCHANGE_LOG_DEBUG(
                "Message rejected reason={}",
                names::reject_reason(rejection.reason)
            );
        }

        return result;
    }

    std::visit(
        Overloaded{
            [](const NewOrderRequest& request) {
                SIXCHANGE_LOG_DEBUG(
                    "New order decoded client_order_id={}, symbol={}, side={}, type={}, tif={}, price={}, quantity={}",
                    request.client_order_id,
                    request.symbol,
                    names::side(request.side),
                    names::order_type(request.order_type),
                    names::time_in_force(request.tif),
                    request.price,
                    request.quantity
                );
            },

            [](const CancelOrderRequest& request) {
                SIXCHANGE_LOG_DEBUG(
                    "Cancel order decoded client_order_id={}, symbol={}",
                    request.client_order_id,
                    request.symbol
                );
            }
        },
        *result
    );

    return result;
}

std::string TextCodec::encode(const OutboundMessage& message) {
    return std::visit(
        Overloaded{
            [](const OrderAccepted& accepted) {
                std::string output{"ACCEPTED "};

                output += std::to_string(
                    accepted.client_order_id
                );

                output += ' ';

                output += std::to_string(
                    accepted.order_id
                );

                return output;
            },

            [](const OrderCancelled& cancelled) {
                std::string output{"CANCELLED "};

                output += std::to_string(
                    cancelled.client_order_id
                );

                output += ' ';

                output += std::to_string(
                    cancelled.order_id
                );

                return output;
            },

            [](const OrderRejected& rejected) {
                std::string output{"REJECTED "};

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
                    names::reject_reason(rejected.reason)
                );

                return output;
            }
        },
        message
    );
}

} // namespace sixchange::protocol
