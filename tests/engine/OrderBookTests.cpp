#include <gtest/gtest.h>

#include <sixchange/core/Commands.h>

#include "engine/OrderBook.h"

namespace sixchange {
namespace {

[[nodiscard]]
constexpr NewOrderCommand make_new_order(
    const SymbolId symbol_id,
    const SequenceNumber sequence_number,
    const ClientOrderId client_order_id,
    const ClientId client_id,
    const Side side,
    const Price price,
    const Quantity quantity) {
    return NewOrderCommand{
        .seq = sequence_number,
        .price = price,
        .quantity = quantity,
        .client_order_id = client_order_id,
        .client_id = client_id,
        .symbol_id = symbol_id,
        .side = side,
        .order_type = OrderType::Limit,
        .tif = TimeInForce::GFD
    };
}

TEST(OrderBookTests, RestsBuyOrderAtCorrectPrice) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    const auto result = book.add(command, OrderId{1});

    ASSERT_TRUE(result.has_value());

    const PriceLevel& level =
        book.bid_level(Price{100});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.front(), nullptr);
    EXPECT_EQ(level.front(), level.head);
    EXPECT_EQ(level.front(), level.tail);

    EXPECT_EQ(level.price, Price{100});
    EXPECT_EQ(level.total_quantity, Quantity{50});

    const Order& order = *level.front();

    EXPECT_EQ(order.order_id, OrderId{1});
    EXPECT_EQ(
        order.client_order_id,
        ClientOrderId{100}
    );
    EXPECT_EQ(order.client_id, ClientId{10});
    EXPECT_EQ(order.symbol_id, symbol_id);
    EXPECT_EQ(order.seq, SequenceNumber{1});
    EXPECT_EQ(order.price, Price{100});
    EXPECT_EQ(
        order.remaining_quantity,
        Quantity{50}
    );
    EXPECT_EQ(order.side, Side::Buy);
    EXPECT_TRUE(order.active);
    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);
}

TEST(OrderBookTests, RestsSellOrderAtCorrectPrice) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{105},
        Quantity{25}
    );

    const auto result = book.add(command, OrderId{1});

    ASSERT_TRUE(result.has_value());

    const PriceLevel& level =
        book.ask_level(Price{105});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.front(), nullptr);
    EXPECT_EQ(level.front(), level.head);
    EXPECT_EQ(level.front(), level.tail);

    EXPECT_EQ(level.price, Price{105});
    EXPECT_EQ(level.total_quantity, Quantity{25});

    const Order& order = *level.front();

    EXPECT_EQ(order.order_id, OrderId{1});
    EXPECT_EQ(
        order.client_order_id,
        ClientOrderId{100}
    );
    EXPECT_EQ(order.client_id, ClientId{10});
    EXPECT_EQ(order.symbol_id, symbol_id);
    EXPECT_EQ(order.seq, SequenceNumber{1});
    EXPECT_EQ(order.price, Price{105});
    EXPECT_EQ(
        order.remaining_quantity,
        Quantity{25}
    );
    EXPECT_EQ(order.side, Side::Sell);
    EXPECT_TRUE(order.active);
    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);
}

TEST(OrderBookTests, PreservesFifoWithinBidPriceLevel) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand first = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    constexpr NewOrderCommand second = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Buy,
        Price{100},
        Quantity{20}
    );

    const auto first_result =
        book.add(first, OrderId{1});

    const auto second_result =
        book.add(second, OrderId{2});

    ASSERT_TRUE(first_result.has_value());
    ASSERT_TRUE(second_result.has_value());

    const PriceLevel& level =
        book.bid_level(Price{100});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.head, nullptr);
    ASSERT_NE(level.tail, nullptr);

    EXPECT_EQ(level.head->order_id, OrderId{1});
    EXPECT_EQ(level.tail->order_id, OrderId{2});

    EXPECT_EQ(
        level.head->client_order_id,
        ClientOrderId{100}
    );

    EXPECT_EQ(
        level.tail->client_order_id,
        ClientOrderId{101}
    );

    ASSERT_NE(level.head->next, nullptr);
    ASSERT_NE(level.tail->prev, nullptr);

    EXPECT_EQ(level.head->prev, nullptr);
    EXPECT_EQ(level.head->next, level.tail);
    EXPECT_EQ(level.tail->prev, level.head);
    EXPECT_EQ(level.tail->next, nullptr);

    EXPECT_EQ(level.total_quantity, Quantity{70});
}

TEST(OrderBookTests, PreservesFifoWithinAskPriceLevel) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand first = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{105},
        Quantity{25}
    );

    constexpr NewOrderCommand second = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Sell,
        Price{105},
        Quantity{30}
    );

    const auto first_result =
        book.add(first, OrderId{1});

    const auto second_result =
        book.add(second, OrderId{2});

    ASSERT_TRUE(first_result.has_value());
    ASSERT_TRUE(second_result.has_value());

    const PriceLevel& level =
        book.ask_level(Price{105});

    ASSERT_NE(level.head, nullptr);
    ASSERT_NE(level.tail, nullptr);

    EXPECT_EQ(level.head->order_id, OrderId{1});
    EXPECT_EQ(level.tail->order_id, OrderId{2});

    EXPECT_EQ(level.head->next, level.tail);
    EXPECT_EQ(level.tail->prev, level.head);

    EXPECT_EQ(level.total_quantity, Quantity{55});
}

TEST(OrderBookTests, KeepsDifferentPricesInSeparateLevels) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand first = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    constexpr NewOrderCommand second = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{10},
        Side::Buy,
        Price{101},
        Quantity{20}
    );

    ASSERT_TRUE(
        book.add(first, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        book.add(second, OrderId{2}).has_value()
    );

    const PriceLevel& first_level =
        book.bid_level(Price{100});

    const PriceLevel& second_level =
        book.bid_level(Price{101});

    ASSERT_NE(first_level.front(), nullptr);
    ASSERT_NE(second_level.front(), nullptr);

    EXPECT_EQ(
        first_level.front()->order_id,
        OrderId{1}
    );

    EXPECT_EQ(
        second_level.front()->order_id,
        OrderId{2}
    );

    EXPECT_EQ(
        first_level.total_quantity,
        Quantity{50}
    );

    EXPECT_EQ(
        second_level.total_quantity,
        Quantity{20}
    );
}

TEST(OrderBookTests, KeepsBidAndAskSidesSeparate) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand buy = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    constexpr NewOrderCommand sell = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{10},
        Side::Sell,
        Price{100},
        Quantity{25}
    );

    ASSERT_TRUE(
        book.add(buy, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        book.add(sell, OrderId{2}).has_value()
    );

    const PriceLevel& bid_level =
        book.bid_level(Price{100});

    const PriceLevel& ask_level =
        book.ask_level(Price{100});

    ASSERT_NE(bid_level.front(), nullptr);
    ASSERT_NE(ask_level.front(), nullptr);

    EXPECT_EQ(
        bid_level.front()->order_id,
        OrderId{1}
    );

    EXPECT_EQ(
        ask_level.front()->order_id,
        OrderId{2}
    );

    EXPECT_EQ(
        bid_level.total_quantity,
        Quantity{50}
    );

    EXPECT_EQ(
        ask_level.total_quantity,
        Quantity{25}
    );
}

/*
 * Rejections
 */

TEST(OrderBookTests, RejectsOrderForDifferentSymbol) {
    constexpr SymbolId book_symbol{1};

    OrderBook book{book_symbol};

    constexpr NewOrderCommand command = make_new_order(
        SymbolId{2},
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    const auto result = book.add(
        command,
        OrderId{1}
    );

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownSymbol
    );

    const PriceLevel& level =
        book.bid_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, Quantity{0});
}

TEST(OrderBookTests, RejectsBuyOrderWithZeroQuantity) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{0}
    );

    const auto result = book.add(
        command,
        OrderId{1}
    );

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::InvalidQuantity
    );

    const PriceLevel& level =
        book.bid_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, Quantity{0});
}

TEST(OrderBookTests, RejectsSellOrderWithZeroQuantity) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{100},
        Quantity{0}
    );

    const auto result = book.add(
        command,
        OrderId{1}
    );

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::InvalidQuantity
    );

    const PriceLevel& level =
        book.ask_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, Quantity{0});
}

TEST(OrderBookTests, RejectsPriceEqualToMaximumTickCount) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{OrderBookMaxPriceTicks},
        Quantity{50}
    );

    const auto result = book.add(
        command,
        OrderId{1}
    );

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::InvalidPrice
    );
}

TEST(OrderBookTests, RejectsPriceAboveMaximumTickCount) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{OrderBookMaxPriceTicks + 1},
        Quantity{50}
    );

    const auto result = book.add(
        command,
        OrderId{1}
    );

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::InvalidPrice
    );
}

TEST(OrderBookTests, AcceptsHighestValidPrice) {
    constexpr SymbolId symbol_id{1};

    constexpr Price highest_valid_price{
        OrderBookMaxPriceTicks - 1
    };

    OrderBook book{symbol_id};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        highest_valid_price,
        Quantity{50}
    );

    const auto result = book.add(
        command,
        OrderId{1}
    );

    ASSERT_TRUE(result.has_value());

    const PriceLevel& level =
        book.bid_level(highest_valid_price);

    ASSERT_NE(level.front(), nullptr);
    EXPECT_EQ(
        level.front()->price,
        highest_valid_price
    );
}

TEST(OrderBookTests, RejectedOrderDoesNotModifyExistingLevel) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id};

    constexpr NewOrderCommand accepted = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    constexpr NewOrderCommand rejected = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{0}
    );

    ASSERT_TRUE(
        book.add(accepted, OrderId{1}).has_value()
    );

    const auto rejected_result =
        book.add(rejected, OrderId{2});

    ASSERT_FALSE(rejected_result.has_value());
    EXPECT_EQ(
        rejected_result.error(),
        RejectReason::InvalidQuantity
    );

    const PriceLevel& level =
        book.bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head, level.tail);
    EXPECT_EQ(level.head->order_id, OrderId{1});
    EXPECT_EQ(level.head->next, nullptr);
    EXPECT_EQ(level.total_quantity, Quantity{50});
}

} // namespace
} // namespace sixchange
