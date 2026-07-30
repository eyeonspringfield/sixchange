#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <string_view>
#include <variant>

#include "protocol/TextCodec.h"

namespace sixchange::protocol {
namespace {

void expect_invalid_message(const std::string_view message) {
    constexpr TextCodec codec;

    const auto result = codec.decode(message);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().reason,
        RejectReason::InvalidMessage
    );
}

void expect_invalid_price(const std::string_view message) {
    constexpr TextCodec codec;

    const auto result = codec.decode(message);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().reason,
        RejectReason::InvalidPrice
    );
}

void expect_invalid_quantity(const std::string_view message) {
    constexpr TextCodec codec;

    const auto result = codec.decode(message);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().reason,
        RejectReason::InvalidQuantity
    );
}

/*
 * Valid new-order messages
 */

TEST(TextCodecTests, DecodesValidNewBuyOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1001 AAPL B L GFD 1852300 100"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request =
        std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);

    EXPECT_EQ(
        request->client_order_id,
        ClientOrderId{1001}
    );
    EXPECT_EQ(request->symbol, "AAPL");
    EXPECT_EQ(request->side, Side::Buy);
    EXPECT_EQ(request->order_type, OrderType::Limit);
    EXPECT_EQ(request->tif, TimeInForce::GFD);
    EXPECT_EQ(request->price, Price{1852300});
    EXPECT_EQ(request->quantity, Quantity{100});
}

TEST(TextCodecTests, DecodesValidNewSellOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1002 MSFT S L GFD 420500 25"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request =
        std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);

    EXPECT_EQ(
        request->client_order_id,
        ClientOrderId{1002}
    );
    EXPECT_EQ(request->symbol, "MSFT");
    EXPECT_EQ(request->side, Side::Sell);
    EXPECT_EQ(request->order_type, OrderType::Limit);
    EXPECT_EQ(request->tif, TimeInForce::GFD);
    EXPECT_EQ(request->price, Price{420500});
    EXPECT_EQ(request->quantity, Quantity{25});
}

TEST(TextCodecTests, DecodesLimitOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 AAPL B L GFD 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->order_type, OrderType::Limit);
}

TEST(TextCodecTests, DecodesMarketOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 AAPL B M GFD 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->order_type, OrderType::Market);
}

TEST(TextCodecTests, DecodesGoodForDayOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 AAPL B L GFD 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->tif, TimeInForce::GFD);
}

TEST(TextCodecTests, DecodesImmediateOrCancelOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 AAPL B L IOC 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->tif, TimeInForce::IOC);
}

TEST(TextCodecTests, DecodesFillOrKillOrder) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 AAPL B L FOK 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->tif, TimeInForce::FOK);
}

TEST(TextCodecTests, PreservesTextualSymbol) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 BRK.B B L GFD 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->symbol, "BRK.B");
}

TEST(TextCodecTests, AcceptsUnknownButSyntacticallyValidSymbol) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1 NOT_LISTED B L GFD 100 5"
    );

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->symbol, "NOT_LISTED");
}

/*
 * Whitespace handling
 */

TEST(TextCodecTests, AcceptsRepeatedSpaces) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N    1001    AAPL    B    L    GFD    100    5"
    );

    ASSERT_TRUE(result.has_value());
}

TEST(TextCodecTests, AcceptsTabsBetweenFields) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N\t1001\tAAPL\tB\tL\tGFD\t100\t5"
    );

    ASSERT_TRUE(result.has_value());
}

TEST(TextCodecTests, AcceptsMixedSpacesAndTabs) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N \t 1001\t AAPL  B\tL GFD\t100 5"
    );

    ASSERT_TRUE(result.has_value());
}

TEST(TextCodecTests, AcceptsLeadingWhitespace) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "   \t N 1001 AAPL B L GFD 100 5"
    );

    ASSERT_TRUE(result.has_value());
}

TEST(TextCodecTests, AcceptsTrailingWhitespace) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1001 AAPL B L GFD 100 5   \t"
    );

    ASSERT_TRUE(result.has_value());
}

TEST(TextCodecTests, AcceptsTrailingCarriageReturn) {
    constexpr TextCodec codec;

    const auto result = codec.decode(
        "N 1001 AAPL B L GFD 100 5\r"
    );

    ASSERT_TRUE(result.has_value());
}

TEST(TextCodecTests, RejectsEmptyMessage) {
    expect_invalid_message("");
}

TEST(TextCodecTests, RejectsWhitespaceOnlyMessage) {
    expect_invalid_message("    \t    ");
}

TEST(TextCodecTests, RejectsUnsupportedMessageType) {
    expect_invalid_message(
        "X 1001 AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsMultiCharacterMessageType) {
    expect_invalid_message(
        "NEW 1001 AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseMessageType) {
    expect_invalid_message(
        "n 1001 AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsMissingClientOrderId) {
    expect_invalid_message("N");
}

TEST(TextCodecTests, RejectsMissingSymbol) {
    expect_invalid_message(
        "N 1001"
    );
}

TEST(TextCodecTests, RejectsMissingSide) {
    expect_invalid_message(
        "N 1001 AAPL"
    );
}

TEST(TextCodecTests, RejectsMissingOrderType) {
    expect_invalid_message(
        "N 1001 AAPL B"
    );
}

TEST(TextCodecTests, RejectsMissingTimeInForce) {
    expect_invalid_message(
        "N 1001 AAPL B L"
    );
}

TEST(TextCodecTests, RejectsMissingPrice) {
    expect_invalid_message(
        "N 1001 AAPL B L GFD"
    );
}

TEST(TextCodecTests, RejectsMissingQuantity) {
    expect_invalid_message(
        "N 1001 AAPL B L GFD 100"
    );
}

TEST(TextCodecTests, RejectsSingleExtraField) {
    expect_invalid_message(
        "N 1001 AAPL B L GFD 100 5 EXTRA"
    );
}

TEST(TextCodecTests, RejectsMultipleExtraFields) {
    expect_invalid_message(
        "N 1001 AAPL B L GFD 100 5 EXTRA MORE"
    );
}

TEST(TextCodecTests, RejectsNonNumericClientOrderId) {
    expect_invalid_message(
        "N ABC AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsPartiallyNumericClientOrderId) {
    expect_invalid_message(
        "N 123ABC AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsNegativeClientOrderId) {
    expect_invalid_message(
        "N -1 AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsZeroClientOrderId) {
    expect_invalid_message(
        "N 0 AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsClientOrderIdWithLeadingPlus) {
    expect_invalid_message(
        "N +1 AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsClientOrderIdOverflow) {
    expect_invalid_message(
        "N 999999999999999999999999999999999999 "
        "AAPL B L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsUnknownSide) {
    expect_invalid_message(
        "N 1001 AAPL X L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseBuySide) {
    expect_invalid_message(
        "N 1001 AAPL b L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseSellSide) {
    expect_invalid_message(
        "N 1001 AAPL s L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsMultiCharacterSide) {
    expect_invalid_message(
        "N 1001 AAPL BUY L GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsUnknownOrderType) {
    expect_invalid_message(
        "N 1001 AAPL B X GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseLimitOrderType) {
    expect_invalid_message(
        "N 1001 AAPL B l GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseMarketOrderType) {
    expect_invalid_message(
        "N 1001 AAPL B m GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsMultiCharacterOrderType) {
    expect_invalid_message(
        "N 1001 AAPL B LIMIT GFD 100 5"
    );
}

TEST(TextCodecTests, RejectsUnknownTimeInForce) {
    expect_invalid_message(
        "N 1001 AAPL B L UNKNOWN 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseGoodForDay) {
    expect_invalid_message(
        "N 1001 AAPL B L gfd 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseImmediateOrCancel) {
    expect_invalid_message(
        "N 1001 AAPL B L ioc 100 5"
    );
}

TEST(TextCodecTests, RejectsLowercaseFillOrKill) {
    expect_invalid_message(
        "N 1001 AAPL B L fok 100 5"
    );
}

TEST(TextCodecTests, RejectsNonNumericPrice) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD ABC 5"
    );
}

TEST(TextCodecTests, RejectsPartiallyNumericPrice) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD 100ABC 5"
    );
}

TEST(TextCodecTests, RejectsNegativePrice) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD -100 5"
    );
}

TEST(TextCodecTests, RejectsZeroPrice) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD 0 5"
    );
}

TEST(TextCodecTests, RejectsPriceWithLeadingPlus) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD +100 5"
    );
}

TEST(TextCodecTests, RejectsDecimalPrice) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD 100.50 5"
    );
}

TEST(TextCodecTests, RejectsPriceOverflow) {
    expect_invalid_price(
        "N 1001 AAPL B L GFD "
        "999999999999999999999999999999999999 5"
    );
}

TEST(TextCodecTests, RejectsNonNumericQuantity) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 ABC"
    );
}

TEST(TextCodecTests, RejectsPartiallyNumericQuantity) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 5ABC"
    );
}

TEST(TextCodecTests, RejectsNegativeQuantity) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 -5"
    );
}

TEST(TextCodecTests, RejectsZeroQuantity) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 0"
    );
}

TEST(TextCodecTests, RejectsQuantityWithLeadingPlus) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 +5"
    );
}

TEST(TextCodecTests, RejectsDecimalQuantity) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 5.5"
    );
}

TEST(TextCodecTests, RejectsQuantityOverflow) {
    expect_invalid_quantity(
        "N 1001 AAPL B L GFD 100 "
        "999999999999999999999999999999999999"
    );
}

TEST(TextCodecTests, AcceptsMaximumClientOrderId) {
    constexpr TextCodec codec;

    const std::string message =
        "N " +
        std::to_string(
            std::numeric_limits<ClientOrderId>::max()
        ) +
        " AAPL B L GFD 100 5";

    const auto result = codec.decode(message);

    ASSERT_TRUE(result.has_value());

    const auto* request = std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(
        request->client_order_id,
        std::numeric_limits<ClientOrderId>::max()
    );
}

TEST(TextCodecTests, AcceptsMaximumPrice) {
    constexpr TextCodec codec;

    const std::string message =
        "N 1 AAPL B L GFD " +
        std::to_string(
            std::numeric_limits<Price>::max()
        ) +
        " 5";

    const auto result = codec.decode(message);

    ASSERT_TRUE(result.has_value());

    const auto* request =
        std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(
        request->price,
        std::numeric_limits<Price>::max()
    );
}

TEST(TextCodecTests, AcceptsMaximumQuantity) {
    constexpr TextCodec codec;

    const std::string message =
        "N 1 AAPL B L GFD 100 " +
        std::to_string(
            std::numeric_limits<Quantity>::max()
        );

    const auto result = codec.decode(message);

    ASSERT_TRUE(result.has_value());

    const auto* request =
        std::get_if<NewOrderRequest>(&*result);

    ASSERT_NE(request, nullptr);
    EXPECT_EQ(
        request->quantity,
        std::numeric_limits<Quantity>::max()
    );
}

TEST(TextCodecTests, DecodesValidCancelOrder)
{
    const auto result =
        TextCodec::decode("C 1001 AAPL");

    ASSERT_TRUE(result.has_value());

    const auto* request =
        std::get_if<CancelOrderRequest>(
            &*result
        );

    ASSERT_NE(request, nullptr);

    EXPECT_EQ(
        request->client_order_id,
        ClientOrderId{1001}
    );
    EXPECT_EQ(request->symbol, "AAPL");
}

TEST(
    TextCodecTests,
    RejectsCancelWithoutClientOrderId)
{
    expect_invalid_message("C");
}

TEST(
    TextCodecTests,
    RejectsCancelWithZeroClientOrderId)
{
    expect_invalid_message("C 0 AAPL");
}

TEST(
    TextCodecTests,
    RejectsCancelWithInvalidClientOrderId)
{
    expect_invalid_message("C abc AAPL");
}

TEST(TextCodecTests, RejectsCancelWithoutSymbol)
{
    const auto result =
        TextCodec::decode("C 1001");

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error().client_order_id,
        ClientOrderId{1001}
    );
    EXPECT_EQ(
        result.error().reason,
        RejectReason::InvalidMessage
    );
}

TEST(
    TextCodecTests,
    RejectsCancelWithTrailingTokens)
{
    const auto result =
        TextCodec::decode(
            "C 1001 AAPL extra"
        );

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error().client_order_id,
        ClientOrderId{1001}
    );
    EXPECT_EQ(
        result.error().reason,
        RejectReason::InvalidMessage
    );
}

TEST(TextCodecTests, EncodesAcceptedOrder)
{
    const OutboundMessage message{
        OrderAccepted{
            .client_order_id =
                ClientOrderId{1001},
            .order_id = OrderId{42}
        }
    };

    EXPECT_EQ(
        TextCodec::encode(message),
        "ACCEPTED 1001 42"
    );
}

TEST(TextCodecTests, EncodesCancelledOrder)
{
    const OutboundMessage message{
        OrderCancelled{
            .client_order_id =
                ClientOrderId{1001},
            .order_id = OrderId{42}
        }
    };

    EXPECT_EQ(
        TextCodec::encode(message),
        "CANCELLED 1001 42"
    );
}

TEST(
    TextCodecTests,
    EncodesRejectedOrderWithClientOrderId)
{
    const OutboundMessage message{
        OrderRejected{
            .client_order_id =
                ClientOrderId{1001},
            .reason =
                RejectReason::UnknownOrder
        }
    };

    EXPECT_EQ(
        TextCodec::encode(message),
        "REJECTED 1001 UNKNOWN_ORDER"
    );
}

TEST(
    TextCodecTests,
    EncodesRejectedOrderWithoutClientOrderId)
{
    const OutboundMessage message{
        OrderRejected{
            .client_order_id =
                std::nullopt,
            .reason =
                RejectReason::InvalidMessage
        }
    };

    EXPECT_EQ(
        TextCodec::encode(message),
        "REJECTED - INVALID_MESSAGE"
    );
}

} // namespace
} // namespace sixchange::protocol
