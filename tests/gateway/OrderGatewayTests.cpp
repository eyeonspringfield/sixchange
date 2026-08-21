#include <gtest/gtest.h>

#include <optional>
#include <string_view>
#include <variant>

#include <sixchange/core/EngineEventDispatcher.h>
#include <sixchange/protocol/OutboundMessageSink.h>

#include "gateway/OrderGateway.h"
#include "TestConfig.h"
#include "TestSinks.h"

namespace sixchange {
namespace {

class OrderGatewayTests : public testing::Test {
protected:
    [[nodiscard]]
    static protocol::NewOrderRequest make_request(
        const ClientOrderId client_order_id = ClientOrderId{1},
        const std::string_view symbol = "AAPL",
        const Side side = Side::Buy,
        const Price price = Price{100},
        const Quantity quantity = Quantity{10},
        const OrderType order_type = OrderType::Limit,
        const TimeInForce tif = TimeInForce::GFD) {
        return protocol::NewOrderRequest{
            .client_order_id = client_order_id,
            .symbol = symbol,
            .side = side,
            .order_type = order_type,
            .tif = tif,
            .price = price,
            .quantity = quantity
        };
    }

    void SetUp() override {
        ASSERT_TRUE(
            dispatcher_.add_sink(EngineEventSink::from(gateway_))
        );
    }

    [[nodiscard]]
    protocol::OutboundMessage submit(
        const protocol::NewOrderRequest& request) {
        outbound_.clear();
        gateway_.handle(protocol::InboundMessage{request});
        return outbound_.take_single();
    }

    [[nodiscard]]
    static protocol::CancelOrderRequest
    make_cancel_request(
        const ClientOrderId client_order_id,
        const std::string_view symbol = "AAPL") {
        return protocol::CancelOrderRequest{
            .client_order_id = client_order_id,
            .symbol = symbol
        };
    }

    [[nodiscard]]
    protocol::OutboundMessage cancel(
        const protocol::CancelOrderRequest& request) {
        outbound_.clear();
        gateway_.handle(protocol::InboundMessage{request});
        return outbound_.take_single();
    }

    EngineEventDispatcher<> dispatcher_{};
    test::OutboundMessageCollector outbound_{};
    MatchingEngine engine_{
        SymbolId{0},
        EngineEventSink::from(dispatcher_),
        test::OrderBookConfig
    };
    OrderGateway gateway_{
        engine_,
        protocol::OutboundMessageSink::from(outbound_)
    };
};

TEST_F(OrderGatewayTests, AcceptsValidAaplOrder) {
    const auto response = submit(
        make_request(ClientOrderId{10})
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(&response);

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(
        accepted->client_order_id,
        ClientOrderId{10}
    );
    EXPECT_EQ(
        accepted->order_id,
        OrderId{1}
    );
}

TEST_F(OrderGatewayTests, RestsBuyOrderOnBidSide) {
    const auto request = make_request(
        ClientOrderId{10},
        "AAPL",
        Side::Buy,
        Price{125},
        Quantity{20}
    );

    const auto response = submit(request);

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(response)
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{125});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head, level.tail);

    EXPECT_EQ(level.price, Price{125});
    EXPECT_EQ(level.total_quantity, Quantity{20});
}

TEST_F(OrderGatewayTests, RestsSellOrderOnAskSide) {
    const auto request = make_request(
        ClientOrderId{10},
        "AAPL",
        Side::Sell,
        Price{125},
        Quantity{20}
    );

    const auto response = submit(request);

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(response)
    );

    const PriceLevel& level =
        engine_.order_book().ask_level(Price{125});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head, level.tail);

    EXPECT_EQ(level.price, Price{125});
    EXPECT_EQ(level.total_quantity, Quantity{20});
}

TEST_F(OrderGatewayTests, ForwardsOrderFieldsToEngine) {
    const auto request = make_request(
        ClientOrderId{135},
        "AAPL",
        Side::Sell,
        Price{750},
        Quantity{42}
    );

    const auto response = submit(request);

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(response)
    );

    const PriceLevel& level =
        engine_.order_book().ask_level(Price{750});

    ASSERT_NE(level.head, nullptr);

    const Order& order = *level.head;

    EXPECT_EQ(order.order_id, OrderId{1});
    EXPECT_EQ(
        order.client_order_id,
        ClientOrderId{135}
    );
    EXPECT_EQ(order.client_id, ClientId{1});
    EXPECT_EQ(order.symbol_id, SymbolId{0});
    EXPECT_EQ(order.side, Side::Sell);
    EXPECT_EQ(order.price, Price{750});
    EXPECT_EQ(
        order.remaining_quantity,
        Quantity{42}
    );
    EXPECT_EQ(order.seq, SequenceNumber{1});
    EXPECT_TRUE(order.active);
}

TEST_F(OrderGatewayTests, UsesDefaultClientId) {
    const auto response = submit(
        make_request(ClientOrderId{10})
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(response)
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head->client_id, ClientId{1});
}

TEST_F(OrderGatewayTests, AssignsIncreasingOrderIds) {
    const auto first_response = submit(
        make_request(
            ClientOrderId{10},
            "AAPL",
            Side::Buy,
            Price{100}
        )
    );

    const auto second_response = submit(
        make_request(
            ClientOrderId{11},
            "AAPL",
            Side::Buy,
            Price{101}
        )
    );

    const auto* first =
        std::get_if<protocol::OrderAccepted>(
            &first_response
        );

    const auto* second =
        std::get_if<protocol::OrderAccepted>(
            &second_response
        );

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_EQ(first->order_id, OrderId{1});
    EXPECT_EQ(second->order_id, OrderId{2});
}

TEST_F(OrderGatewayTests, AssignsIncreasingSequenceNumbers) {
    const auto first_response = submit(
        make_request(
            ClientOrderId{10},
            "AAPL",
            Side::Buy,
            Price{100}
        )
    );

    const auto second_response = submit(
        make_request(
            ClientOrderId{11},
            "AAPL",
            Side::Buy,
            Price{101}
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(first_response)
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(second_response)
    );

    const PriceLevel& first_level =
        engine_.order_book().bid_level(Price{100});

    const PriceLevel& second_level =
        engine_.order_book().bid_level(Price{101});

    ASSERT_NE(first_level.head, nullptr);
    ASSERT_NE(second_level.head, nullptr);

    EXPECT_EQ(
        first_level.head->seq,
        SequenceNumber{1}
    );

    EXPECT_EQ(
        second_level.head->seq,
        SequenceNumber{2}
    );
}

TEST_F(OrderGatewayTests, UsesSequenceNumberFromSharedSequencer) {
    EXPECT_EQ(
        Sequencer::instance().next(),
        SequenceNumber{1}
    );

    const auto response = submit(
        make_request(ClientOrderId{10})
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(response)
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head->seq, SequenceNumber{2});
}

TEST_F(OrderGatewayTests, ReturnedOrderIdMatchesRestingOrderId) {
    const auto response = submit(
        make_request(ClientOrderId{10})
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(&response);

    ASSERT_NE(accepted, nullptr);

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);

    EXPECT_EQ(
        accepted->order_id,
        level.head->order_id
    );
}

TEST_F(OrderGatewayTests, RejectsUnknownSymbol) {
    const auto response = submit(
        make_request(
            ClientOrderId{10},
            "UNKNOWN"
        )
    );

    const auto* rejected =
        std::get_if<protocol::OrderRejected>(&response);

    ASSERT_NE(rejected, nullptr);

    ASSERT_TRUE(
        rejected->client_order_id.has_value()
    );

    EXPECT_EQ(
        *rejected->client_order_id,
        ClientOrderId{10}
    );

    EXPECT_EQ(
        rejected->reason,
        RejectReason::UnknownSymbol
    );
}

TEST_F(OrderGatewayTests, UnknownSymbolDoesNotModifyOrderBook) {
    const auto response = submit(
        make_request(
            ClientOrderId{10},
            "UNKNOWN",
            Side::Buy,
            Price{100}
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(response)
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, Quantity{0});
}

TEST_F(OrderGatewayTests, UnknownSymbolDoesNotConsumeOrderId) {
    const auto rejected = submit(
        make_request(
            ClientOrderId{10},
            "UNKNOWN"
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(rejected)
    );

    const auto accepted_response = submit(
        make_request(
            ClientOrderId{11},
            "AAPL"
        )
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(
            &accepted_response
        );

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(accepted->order_id, OrderId{1});
}

TEST_F(OrderGatewayTests, UnknownSymbolDoesNotConsumeSequenceNumber) {
    const auto rejected = submit(
        make_request(
            ClientOrderId{10},
            "UNKNOWN"
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(rejected)
    );

    const auto accepted = submit(
        make_request(
            ClientOrderId{11},
            "AAPL",
            Side::Buy,
            Price{100}
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(accepted)
    );

    const PriceLevel& level =
        engine_.order_book().bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head->seq, SequenceNumber{1});
}

TEST_F(OrderGatewayTests, UnknownSymbolDoesNotReserveClientOrderId) {
    const auto rejected = submit(
        make_request(
            ClientOrderId{10},
            "UNKNOWN"
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(rejected)
    );

    const auto accepted = submit(
        make_request(
            ClientOrderId{10},
            "AAPL"
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(accepted)
    );
}

TEST_F(OrderGatewayTests, RejectsDuplicateClientOrderId) {
    const auto first = submit(
        make_request(ClientOrderId{10})
    );

    const auto duplicate = submit(
        make_request(ClientOrderId{10})
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(first)
    );

    const auto* rejected =
        std::get_if<protocol::OrderRejected>(
            &duplicate
        );

    ASSERT_NE(rejected, nullptr);

    ASSERT_TRUE(
        rejected->client_order_id.has_value()
    );

    EXPECT_EQ(
        *rejected->client_order_id,
        ClientOrderId{10}
    );

    EXPECT_EQ(
        rejected->reason,
        RejectReason::DuplicateClientOrderId
    );
}

TEST_F(OrderGatewayTests, DuplicateClientOrderIdDoesNotRestSecondOrder) {
    const auto first = submit(
        make_request(
            ClientOrderId{10},
            "AAPL",
            Side::Buy,
            Price{100},
            Quantity{10}
        )
    );

    const auto duplicate = submit(
        make_request(
            ClientOrderId{10},
            "AAPL",
            Side::Buy,
            Price{101},
            Quantity{20}
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(first)
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(duplicate)
    );

    const PriceLevel& original_level =
        engine_.order_book().bid_level(Price{100});

    const PriceLevel& duplicate_level =
        engine_.order_book().bid_level(Price{101});

    EXPECT_FALSE(original_level.empty());
    EXPECT_EQ(
        original_level.total_quantity,
        Quantity{10}
    );

    EXPECT_TRUE(duplicate_level.empty());
    EXPECT_EQ(
        duplicate_level.total_quantity,
        Quantity{0}
    );
}

TEST_F(OrderGatewayTests, DuplicateClientOrderIdDoesNotConsumeOrderId) {
    const auto first = submit(
        make_request(ClientOrderId{10})
    );

    const auto duplicate = submit(
        make_request(ClientOrderId{10})
    );

    const auto second = submit(
        make_request(
            ClientOrderId{11},
            "AAPL",
            Side::Buy,
            Price{101}
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(first)
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(duplicate)
    );

    const auto* accepted =
        std::get_if<protocol::OrderAccepted>(&second);

    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(accepted->order_id, OrderId{2});
}

TEST_F(OrderGatewayTests, DuplicateClientOrderIdDoesNotConsumeSequenceNumber) {
    const auto first = submit(
        make_request(
            ClientOrderId{10},
            "AAPL",
            Side::Buy,
            Price{100}
        )
    );

    const auto duplicate = submit(
        make_request(
            ClientOrderId{10},
            "AAPL",
            Side::Buy,
            Price{101}
        )
    );

    const auto second = submit(
        make_request(
            ClientOrderId{11},
            "AAPL",
            Side::Buy,
            Price{102}
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(first)
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderRejected
        >(duplicate)
    );

    ASSERT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(second)
    );

    const PriceLevel& second_level =
        engine_.order_book().bid_level(Price{102});

    ASSERT_NE(second_level.head, nullptr);
    EXPECT_EQ(
        second_level.head->seq,
        SequenceNumber{2}
    );
}

TEST_F(OrderGatewayTests, AcceptsDifferentClientOrderIds) {
    const auto first = submit(
        make_request(ClientOrderId{10})
    );

    const auto second = submit(
        make_request(
            ClientOrderId{11},
            "AAPL",
            Side::Buy,
            Price{101}
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(first)
    );

    EXPECT_TRUE(
        std::holds_alternative<
        protocol::OrderAccepted
        >(second)
    );
}

TEST_F(
    OrderGatewayTests,
    CancelsAcceptedOrder)
{
    const auto accepted = submit(
        make_request(
            ClientOrderId{100},
            "AAPL",
            Side::Buy,
            Price{100},
            Quantity{10}
        )
    );

    ASSERT_NE(
        std::get_if<
            protocol::OrderAccepted
        >(&accepted),
        nullptr
    );

    const auto response = cancel(
        make_cancel_request(
            ClientOrderId{100}
        )
    );

    const auto* cancelled =
        std::get_if<
            protocol::OrderCancelled
        >(&response);

    ASSERT_NE(cancelled, nullptr);

    EXPECT_EQ(
        cancelled->client_order_id,
        ClientOrderId{100}
    );
    EXPECT_EQ(
        cancelled->order_id,
        OrderId{1}
    );

    EXPECT_TRUE(
        engine_.order_book()
            .bid_level(Price{100})
            .empty()
    );
}

TEST_F(
    OrderGatewayTests,
    RejectsUnknownClientOrderCancellation)
{
    const auto response = cancel(
        make_cancel_request(
            ClientOrderId{999}
        )
    );

    const auto* rejected =
        std::get_if<
            protocol::OrderRejected
        >(&response);

    ASSERT_NE(rejected, nullptr);

    EXPECT_EQ(
        rejected->client_order_id,
        ClientOrderId{999}
    );
    EXPECT_EQ(
        rejected->reason,
        RejectReason::UnknownOrder
    );
}

TEST_F(
    OrderGatewayTests,
    RejectsCancellationForUnknownSymbol)
{
    const auto accepted = submit(
        make_request(ClientOrderId{100})
    );

    ASSERT_NE(
        std::get_if<
            protocol::OrderAccepted
        >(&accepted),
        nullptr
    );

    const auto response = cancel(
        make_cancel_request(
            ClientOrderId{100},
            "MSFT"
        )
    );

    const auto* rejected =
        std::get_if<
            protocol::OrderRejected
        >(&response);

    ASSERT_NE(rejected, nullptr);

    EXPECT_EQ(
        rejected->reason,
        RejectReason::UnknownSymbol
    );

    EXPECT_FALSE(
        engine_.order_book()
            .bid_level(Price{100})
            .empty()
    );
}

TEST_F(
    OrderGatewayTests,
    RejectsSecondCancellation)
{
    const auto accepted = submit(
        make_request(ClientOrderId{100})
    );

    ASSERT_NE(
        std::get_if<protocol::OrderAccepted>(
            &accepted
        ),
        nullptr
    );

    const auto first_cancel = cancel(
        make_cancel_request(
            ClientOrderId{100}
        )
    );

    ASSERT_NE(
        std::get_if<
            protocol::OrderCancelled
        >(&first_cancel),
        nullptr
    );

    const auto second_cancel = cancel(
        make_cancel_request(
            ClientOrderId{100}
        )
    );

    const auto* rejected =
        std::get_if<
            protocol::OrderRejected
        >(&second_cancel);

    ASSERT_NE(rejected, nullptr);

    EXPECT_EQ(
        rejected->reason,
        RejectReason::UnknownOrder
    );
}

TEST_F(
    OrderGatewayTests,
    KeepsCancelledClientOrderIdReserved)
{
    const auto accepted = submit(
        make_request(ClientOrderId{100})
    );

    ASSERT_NE(
        std::get_if<
            protocol::OrderAccepted
        >(&accepted),
        nullptr
    );

    const auto cancelled = cancel(
        make_cancel_request(
            ClientOrderId{100}
        )
    );

    ASSERT_NE(
        std::get_if<
            protocol::OrderCancelled
        >(&cancelled),
        nullptr
    );

    const auto reused = submit(
        make_request(
            ClientOrderId{100},
            "AAPL",
            Side::Buy,
            Price{101}
        )
    );

    const auto* rejected =
        std::get_if<
            protocol::OrderRejected
        >(&reused);

    ASSERT_NE(rejected, nullptr);

    EXPECT_EQ(
        rejected->reason,
        RejectReason::
            DuplicateClientOrderId
    );
}

} // namespace
} // namespace sixchange
