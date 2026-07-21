#include <gtest/gtest.h>

#include <string_view>
#include <variant>

#include "engine/MatchingEngine.h"
#include "engine/Sequencer.h"
#include "gateway/OrderGateway.h"
#include "protocol/TextCodec.h"

namespace sixchange {
namespace {

class OrderFlowIntegrationTests : public testing::Test {
protected:
    [[nodiscard]] protocol::OutboundMessage submit_text(const std::string_view text) {
        const auto decoded = codec_.decode(text);

        if (!decoded) {
            return protocol::OutboundMessage{decoded.error()};
        }

        return gateway_.handle(*decoded);
    }

    protocol::TextCodec codec_{};
    MatchingEngine engine_{SymbolId{0}};
    OrderGateway gateway_{engine_};
};

TEST_F(OrderFlowIntegrationTests, DecodesAcceptsAndRestsBuyOrder) {
    const auto response = submit_text(
        "N 1001 AAPL B L GFD 125 20"
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(&response);

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(
        accepted->client_order_id,
        ClientOrderId{1001}
    );
    EXPECT_EQ(
        accepted->order_id,
        OrderId{1}
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{125});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.head, nullptr);
    ASSERT_EQ(level.head, level.tail);

    const Order& order = *level.head;

    EXPECT_EQ(level.price, Price{125});
    EXPECT_EQ(
        level.total_quantity,
        Quantity{20}
    );

    EXPECT_EQ(
        order.order_id,
        accepted->order_id
    );
    EXPECT_EQ(
        order.client_order_id,
        ClientOrderId{1001}
    );
    EXPECT_EQ(
        order.client_id,
        ClientId{1}
    );
    EXPECT_EQ(
        order.symbol_id,
        SymbolId{0}
    );
    EXPECT_EQ(
        order.side,
        Side::Buy
    );
    EXPECT_EQ(
        order.price,
        Price{125}
    );
    EXPECT_EQ(
        order.remaining_quantity,
        Quantity{20}
    );
    EXPECT_EQ(
        order.seq,
        SequenceNumber{1}
    );
    EXPECT_TRUE(order.active);
    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);
}

TEST_F(OrderFlowIntegrationTests, DecodesAcceptsAndRestsSellOrder) {
    const auto response = submit_text(
        "N 2001 AAPL S L GFD 130 15"
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(&response);

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(
        accepted->client_order_id,
        ClientOrderId{2001}
    );
    EXPECT_EQ(
        accepted->order_id,
        OrderId{1}
    );

    const PriceLevel& ask_level =
        engine_.order_book().ask_level(Price{130});

    const PriceLevel& bid_level =
        engine_.order_book().bid_level(Price{130});

    ASSERT_FALSE(ask_level.empty());
    ASSERT_NE(ask_level.head, nullptr);

    EXPECT_EQ(
        ask_level.total_quantity,
        Quantity{15}
    );
    EXPECT_EQ(
        ask_level.head->side,
        Side::Sell
    );
    EXPECT_EQ(
        ask_level.head->seq,
        SequenceNumber{1}
    );

    EXPECT_TRUE(bid_level.empty());
    EXPECT_EQ(
        bid_level.total_quantity,
        Quantity{0}
    );
}

TEST_F(
    OrderFlowIntegrationTests,
    PreservesFifoAtSamePriceAcrossTextMessages
) {
    const auto first_response = submit_text(
        "N 3001 AAPL B L GFD 100 10"
    );

    const auto second_response = submit_text(
        "N 3002 AAPL B L GFD 100 20"
    );

    const auto* first_accepted =
        std::get_if<protocol::OrderAccepted>(
            &first_response
        );

    const auto* second_accepted =
        std::get_if<protocol::OrderAccepted>(
            &second_response
        );

    ASSERT_NE(first_accepted, nullptr);
    ASSERT_NE(second_accepted, nullptr);

    EXPECT_EQ(
        first_accepted->order_id,
        OrderId{1}
    );
    EXPECT_EQ(
        second_accepted->order_id,
        OrderId{2}
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);
    ASSERT_NE(level.tail, nullptr);
    ASSERT_NE(level.head, level.tail);

    EXPECT_EQ(
        level.total_quantity,
        Quantity{30}
    );

    EXPECT_EQ(
        level.head->client_order_id,
        ClientOrderId{3001}
    );
    EXPECT_EQ(
        level.head->order_id,
        OrderId{1}
    );
    EXPECT_EQ(
        level.head->seq,
        SequenceNumber{1}
    );
    EXPECT_EQ(level.head->prev, nullptr);
    EXPECT_EQ(level.head->next, level.tail);

    EXPECT_EQ(
        level.tail->client_order_id,
        ClientOrderId{3002}
    );
    EXPECT_EQ(
        level.tail->order_id,
        OrderId{2}
    );
    EXPECT_EQ(
        level.tail->seq,
        SequenceNumber{2}
    );
    EXPECT_EQ(level.tail->prev, level.head);
    EXPECT_EQ(level.tail->next, nullptr);
}

TEST_F(
    OrderFlowIntegrationTests,
    CodecRejectionDoesNotReachGatewayOrConsumeState
) {
    const auto rejected_response = submit_text(
        "N 4001 AAPL B L GFD 100 0"
    );

    const auto* rejected =
        std::get_if<protocol::OrderRejected>(
            &rejected_response
        );

    ASSERT_NE(rejected, nullptr);
    EXPECT_FALSE(
        rejected->client_order_id.has_value()
    );
    EXPECT_EQ(
        rejected->reason,
        RejectReason::InvalidQuantity
    );

    const PriceLevel& rejected_level =
        engine_.order_book().bid_level(Price{100});

    EXPECT_TRUE(rejected_level.empty());
    EXPECT_EQ(
        rejected_level.total_quantity,
        Quantity{0}
    );

    const auto accepted_response = submit_text(
        "N 4001 AAPL B L GFD 101 5"
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(
            &accepted_response
        );

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(
        accepted->order_id,
        OrderId{1}
    );

    const PriceLevel& accepted_level =
        engine_.order_book().bid_level(Price{101});

    ASSERT_NE(accepted_level.head, nullptr);
    EXPECT_EQ(
        accepted_level.head->seq,
        SequenceNumber{1}
    );
}

TEST_F(
    OrderFlowIntegrationTests,
    UnknownSymbolDoesNotMutateOrConsumeGatewayState
) {
    const auto rejected_response = submit_text(
        "N 5001 UNKNOWN B L GFD 100 10"
    );

    const auto* rejected =
        std::get_if<protocol::OrderRejected>(
            &rejected_response
        );

    ASSERT_NE(rejected, nullptr);
    ASSERT_TRUE(
        rejected->client_order_id.has_value()
    );
    EXPECT_EQ(
        *rejected->client_order_id,
        ClientOrderId{5001}
    );
    EXPECT_EQ(
        rejected->reason,
        RejectReason::UnknownSymbol
    );

    const PriceLevel& rejected_level =
        engine_.order_book().bid_level(Price{100});

    EXPECT_TRUE(rejected_level.empty());
    EXPECT_EQ(
        rejected_level.total_quantity,
        Quantity{0}
    );

    // The rejected request must not reserve its client order ID,
    // consume an order ID, or consume a sequence number.
    const auto accepted_response = submit_text(
        "N 5001 AAPL B L GFD 101 10"
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(
            &accepted_response
        );

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(
        accepted->order_id,
        OrderId{1}
    );

    const PriceLevel& accepted_level =
        engine_.order_book().bid_level(Price{101});

    ASSERT_NE(accepted_level.head, nullptr);
    EXPECT_EQ(
        accepted_level.head->seq,
        SequenceNumber{1}
    );
}

TEST_F(
    OrderFlowIntegrationTests,
    DuplicateClientOrderIdDoesNotAddOrConsumeState
) {
    const auto first_response = submit_text(
        "N 6001 AAPL B L GFD 100 10"
    );

    const auto duplicate_response = submit_text(
        "N 6001 AAPL B L GFD 101 20"
    );

    const auto second_response = submit_text(
        "N 6002 AAPL B L GFD 102 30"
    );

    const auto* first_accepted =
        std::get_if<protocol::OrderAccepted>(
            &first_response
        );

    const auto* duplicate_rejected =
        std::get_if<protocol::OrderRejected>(
            &duplicate_response
        );

    const auto* second_accepted =
        std::get_if<protocol::OrderAccepted>(
            &second_response
        );

    ASSERT_NE(first_accepted, nullptr);
    ASSERT_NE(duplicate_rejected, nullptr);
    ASSERT_NE(second_accepted, nullptr);

    EXPECT_EQ(
        first_accepted->order_id,
        OrderId{1}
    );
    EXPECT_EQ(
        duplicate_rejected->reason,
        RejectReason::DuplicateClientOrderId
    );
    EXPECT_EQ(
        second_accepted->order_id,
        OrderId{2}
    );

    const PriceLevel& first_level =
        engine_.order_book().bid_level(Price{100});

    const PriceLevel& duplicate_level =
        engine_.order_book().bid_level(Price{101});

    const PriceLevel& second_level =
        engine_.order_book().bid_level(Price{102});

    ASSERT_NE(first_level.head, nullptr);
    EXPECT_EQ(
        first_level.total_quantity,
        Quantity{10}
    );
    EXPECT_EQ(
        first_level.head->seq,
        SequenceNumber{1}
    );

    EXPECT_TRUE(duplicate_level.empty());
    EXPECT_EQ(
        duplicate_level.total_quantity,
        Quantity{0}
    );

    ASSERT_NE(second_level.head, nullptr);
    EXPECT_EQ(
        second_level.total_quantity,
        Quantity{30}
    );
    EXPECT_EQ(
        second_level.head->seq,
        SequenceNumber{2}
    );
}

} // namespace
} // namespace sixchange
