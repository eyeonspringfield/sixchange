#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <expected>

#include <sixchange/core/Commands.h>

#include "engine/OrderBook.h"
#include "TestConfig.h"

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

[[nodiscard]]
constexpr CancelOrderCommand make_cancel_order(
    const SymbolId symbol_id,
    const SequenceNumber sequence_number,
    const OrderId order_id,
    const ClientOrderId client_order_id,
    const ClientId client_id)
{
    return CancelOrderCommand{
        .seq = sequence_number,
        .order_id = order_id,
        .client_order_id = client_order_id,
        .client_id = client_id,
        .symbol_id = symbol_id
    };
}

struct IgnoreMatches {
    void on_match(const MatchExecution&) noexcept {
    }
};

[[nodiscard]]
std::expected<NewOrderOutcome, RejectReason> submit_order(
    OrderBook& book,
    const NewOrderCommand& command,
    const OrderId order_id) noexcept {
    const auto admitted = book.admit(command, order_id);

    if (!admitted) {
        return std::unexpected{admitted.error()};
    }

    IgnoreMatches sink;
    return book.execute(*admitted, MatchSink::from(sink));
}

TEST(OrderBookTests, RestsBuyOrderAtCorrectPrice) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    const auto result = submit_order(book, command, OrderId{1});

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

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{105},
        Quantity{25}
    );

    const auto result = submit_order(book, command, OrderId{1});

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

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        submit_order(book, first, OrderId{1});

    const auto second_result =
        submit_order(book, second, OrderId{2});

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

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        submit_order(book, first, OrderId{1});

    const auto second_result =
        submit_order(book, second, OrderId{2});

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

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        submit_order(book, first, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, second, OrderId{2}).has_value()
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

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        Price{101}, // non-crossing
        Quantity{25}
    );

    ASSERT_TRUE(
        submit_order(book, buy, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, sell, OrderId{2}).has_value()
    );

    const PriceLevel& bid_level =
        book.bid_level(Price{100});

    const PriceLevel& ask_level =
        book.ask_level(Price{101});

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

    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());

    EXPECT_EQ(*book.best_bid(), Price{100});
    EXPECT_EQ(*book.best_ask(), Price{101});
}

/*
 * Rejections
 */

TEST(OrderBookTests, RejectsOrderForDifferentSymbol) {
    constexpr SymbolId book_symbol{1};

    OrderBook book{book_symbol, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        SymbolId{2},
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    const auto result = submit_order(book,
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

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{0}
    );

    const auto result = submit_order(book,
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

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{100},
        Quantity{0}
    );

    const auto result = submit_order(book,
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

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{test::OrderBookConfig.price_level_count},
        Quantity{50}
    );

    const auto result = submit_order(book,
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

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{test::OrderBookConfig.price_level_count + 1},
        Quantity{50}
    );

    const auto result = submit_order(book,
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
        test::OrderBookConfig.price_level_count - 1
    };

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        highest_valid_price,
        Quantity{50}
    );

    const auto result = submit_order(book,
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

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        submit_order(book, accepted, OrderId{1}).has_value()
    );

    const auto rejected_result =
        submit_order(book, rejected, OrderId{2});

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

TEST(
    OrderBookTests,
    UsesRuntimeConfiguredOrderCapacity)
{
    constexpr SymbolId symbol_id{1};

    constexpr OrderBookConfig config{
        .max_active_orders = 2,
        .price_level_count = 128,
        .order_lookup_capacity = 4
    };

    OrderBook book{symbol_id, config};

    const auto first = submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{1},
            ClientId{10},
            Side::Buy,
            Price{10},
            Quantity{1}
        ),
        OrderId{1}
    );

    const auto second = submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{2},
            ClientId{10},
            Side::Buy,
            Price{11},
            Quantity{1}
        ),
        OrderId{2}
    );

    const auto third = submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{3},
            ClientId{10},
            Side::Buy,
            Price{12},
            Quantity{1}
        ),
        OrderId{3}
    );

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_FALSE(third.has_value());

    EXPECT_EQ(
        third.error(),
        RejectReason::CapacityExhausted
    );
}

TEST(
    OrderBookTests,
    RollsBackPoolAllocationForDuplicateOrderId)
{
    constexpr SymbolId symbol_id{1};

    constexpr OrderBookConfig config{
        .max_active_orders = 2,
        .price_level_count = 128,
        .order_lookup_capacity = 4
    };

    OrderBook book{symbol_id, config};

    ASSERT_TRUE(
        submit_order(book,
            make_new_order(
                symbol_id,
                SequenceNumber{1},
                ClientOrderId{100},
                ClientId{10},
                Side::Buy,
                Price{100},
                Quantity{10}
            ),
            OrderId{1}
        ).has_value()
    );

    const auto duplicate = submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{10},
            Side::Buy,
            Price{101},
            Quantity{10}
        ),
        OrderId{1}
    );

    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(
        duplicate.error(),
        RejectReason::MatchingEngineError
    );

    EXPECT_TRUE(
        book.bid_level(Price{101}).empty()
    );

    const auto next = submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{10},
            Side::Buy,
            Price{102},
            Quantity{10}
        ),
        OrderId{2}
    );

    EXPECT_TRUE(next.has_value());
}

TEST(
    OrderBookTests,
    CancelsOnlyBuyOrderAtLevel)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(
        submit_order(book,
            make_new_order(
                symbol_id,
                SequenceNumber{1},
                ClientOrderId{100},
                ClientId{10},
                Side::Buy,
                Price{100},
                Quantity{50}
            ),
            OrderId{1}
        ).has_value()
    );

    const auto result = book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100},
            ClientId{10}
        )
    );

    ASSERT_TRUE(result.has_value());

    const PriceLevel& level =
        book.bid_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    EXPECT_EQ(
        level.total_quantity,
        Quantity{0}
    );
    EXPECT_EQ(book.best_bid(), std::nullopt);
}

TEST(
    OrderBookTests,
    CancelsOnlySellOrderAtLevel)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(
        submit_order(book,
            make_new_order(
                symbol_id,
                SequenceNumber{1},
                ClientOrderId{100},
                ClientId{10},
                Side::Sell,
                Price{105},
                Quantity{25}
            ),
            OrderId{1}
        ).has_value()
    );

    ASSERT_TRUE(
        book.cancel(
            make_cancel_order(
                symbol_id,
                SequenceNumber{2},
                OrderId{1},
                ClientOrderId{100},
                ClientId{10}
            )
        ).has_value()
    );

    const PriceLevel& level =
        book.ask_level(Price{105});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(
        level.total_quantity,
        Quantity{0}
    );
    EXPECT_EQ(book.best_ask(), std::nullopt);
}

TEST(
    OrderBookTests,
    CancelsMiddleOrderWithoutBreakingLinks)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{10}
        ),
        OrderId{1}
    ).has_value());

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{20}
        ),
        OrderId{2}
    ).has_value());

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{30}
        ),
        OrderId{3}
    ).has_value());

    ASSERT_TRUE(book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{4},
            OrderId{2},
            ClientOrderId{101},
            ClientId{10}
        )
    ).has_value());

    const PriceLevel& level =
        book.bid_level(Price{100});

    ASSERT_NE(level.head, nullptr);
    ASSERT_NE(level.tail, nullptr);

    EXPECT_EQ(
        level.head->order_id,
        OrderId{1}
    );
    EXPECT_EQ(
        level.tail->order_id,
        OrderId{3}
    );
    EXPECT_EQ(level.head->next, level.tail);
    EXPECT_EQ(level.tail->prev, level.head);
    EXPECT_EQ(
        level.total_quantity,
        Quantity{40}
    );
}

TEST(
    OrderBookTests,
    RejectsUnknownOrderCancellation)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    const auto result = book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{1},
            OrderId{999},
            ClientOrderId{100},
            ClientId{10}
        )
    );

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownOrder
    );
}

TEST(
    OrderBookTests,
    RejectsCancellationByWrongOwner)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{10}
        ),
        OrderId{1}
    ).has_value());

    const auto result = book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100},
            ClientId{11}
        )
    );

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::NotOrderOwner
    );

    EXPECT_FALSE(
        book.bid_level(Price{100}).empty()
    );
}

TEST(
    OrderBookTests,
    RejectsCancellationWithWrongClientOrderId)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{10}
        ),
        OrderId{1}
    ).has_value());

    const auto result = book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{101},
            ClientId{10}
        )
    );

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::NotOrderOwner
    );
}

TEST(
    OrderBookTests,
    RejectsCancellationForWrongSymbol)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    const auto result = book.cancel(
        make_cancel_order(
            SymbolId{2},
            SequenceNumber{1},
            OrderId{1},
            ClientOrderId{100},
            ClientId{10}
        )
    );

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownSymbol
    );
}

TEST(
    OrderBookTests,
    UpdatesBestBidAfterCancellation)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{10}
        ),
        OrderId{1}
    ).has_value());

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{10},
            Side::Buy,
            Price{105},
            Quantity{10}
        ),
        OrderId{2}
    ).has_value());

    ASSERT_EQ(book.best_bid(), Price{105});

    ASSERT_TRUE(book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{3},
            OrderId{2},
            ClientOrderId{101},
            ClientId{10}
        )
    ).has_value());

    EXPECT_EQ(book.best_bid(), Price{100});
}

TEST(
    OrderBookTests,
    UpdatesBestAskAfterCancellation)
{
    constexpr SymbolId symbol_id{1};

    OrderBook book{
        symbol_id,
        test::OrderBookConfig
    };

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Sell,
            Price{105},
            Quantity{10}
        ),
        OrderId{1}
    ).has_value());

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{10},
            Side::Sell,
            Price{100},
            Quantity{10}
        ),
        OrderId{2}
    ).has_value());

    ASSERT_EQ(book.best_ask(), Price{100});

    ASSERT_TRUE(book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{3},
            OrderId{2},
            ClientOrderId{101},
            ClientId{10}
        )
    ).has_value());

    EXPECT_EQ(book.best_ask(), Price{105});
}

TEST(
    OrderBookTests,
    ReusesPoolCapacityAfterCancellation)
{
    constexpr SymbolId symbol_id{1};

    constexpr OrderBookConfig config{
        .max_active_orders = 1,
        .price_level_count = 128,
        .order_lookup_capacity = 2
    };

    OrderBook book{symbol_id, config};

    ASSERT_TRUE(submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{10}
        ),
        OrderId{1}
    ).has_value());

    ASSERT_TRUE(book.cancel(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100},
            ClientId{10}
        )
    ).has_value());

    const auto second = submit_order(book,
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{101},
            ClientId{10},
            Side::Buy,
            Price{101},
            Quantity{20}
        ),
        OrderId{2}
    );

    ASSERT_TRUE(second.has_value());

    ASSERT_NE(
        book.bid_level(Price{101}).head,
        nullptr
    );

    EXPECT_EQ(
        book.bid_level(Price{101})
            .head->order_id,
        OrderId{2}
    );
}

TEST(OrderBookTests, BuyBelowBestAskDoesNotMatch) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{105},
        Quantity{50}
    );

    constexpr NewOrderCommand buy = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Buy,
        Price{104},
        Quantity{30}
    );

    ASSERT_TRUE(submit_order(book, sell, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, buy, OrderId{2}).has_value());

    const PriceLevel& ask =
        book.ask_level(Price{105});

    const PriceLevel& bid =
        book.bid_level(Price{104});

    ASSERT_NE(ask.front(), nullptr);
    ASSERT_NE(bid.front(), nullptr);

    EXPECT_EQ(
        ask.front()->remaining_quantity,
        Quantity{50}
    );

    EXPECT_EQ(
        bid.front()->remaining_quantity,
        Quantity{30}
    );

    EXPECT_EQ(ask.total_quantity, Quantity{50});
    EXPECT_EQ(bid.total_quantity, Quantity{30});

    ASSERT_TRUE(book.best_ask().has_value());
    ASSERT_TRUE(book.best_bid().has_value());

    EXPECT_EQ(*book.best_ask(), Price{105});
    EXPECT_EQ(*book.best_bid(), Price{104});
}


TEST(OrderBookTests, BuyExactlyFillsSingleRestingAsk) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{100},
        Quantity{50}
    );

    constexpr NewOrderCommand buy = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    ASSERT_TRUE(submit_order(book, sell, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, buy, OrderId{2}).has_value());

    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );

    EXPECT_TRUE(
        book.bid_level(Price{100}).empty()
    );

    EXPECT_EQ(
        book.ask_level(Price{100}).total_quantity,
        Quantity{0}
    );

    EXPECT_EQ(
        book.bid_level(Price{100}).total_quantity,
        Quantity{0}
    );

    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.best_bid().has_value());
}


TEST(OrderBookTests, BuyPartiallyFillsRestingAsk) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{100},
        Quantity{50}
    );

    constexpr NewOrderCommand buy = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Buy,
        Price{100},
        Quantity{30}
    );

    ASSERT_TRUE(submit_order(book, sell, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, buy, OrderId{2}).has_value());

    const PriceLevel& ask =
        book.ask_level(Price{100});

    ASSERT_NE(ask.front(), nullptr);

    EXPECT_EQ(
        ask.front()->order_id,
        OrderId{1}
    );

    EXPECT_EQ(
        ask.front()->remaining_quantity,
        Quantity{20}
    );

    EXPECT_EQ(
        ask.total_quantity,
        Quantity{20}
    );

    EXPECT_TRUE(
        book.bid_level(Price{100}).empty()
    );

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), Price{100});

    EXPECT_FALSE(book.best_bid().has_value());
}


TEST(OrderBookTests, BuyConsumesAskAndRestsRemainder) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Sell,
        Price{100},
        Quantity{30}
    );

    constexpr NewOrderCommand buy = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Buy,
        Price{100},
        Quantity{50}
    );

    ASSERT_TRUE(submit_order(book, sell, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, buy, OrderId{2}).has_value());

    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );

    const PriceLevel& bid =
        book.bid_level(Price{100});

    ASSERT_NE(bid.front(), nullptr);

    EXPECT_EQ(
        bid.front()->order_id,
        OrderId{2}
    );

    EXPECT_EQ(
        bid.front()->remaining_quantity,
        Quantity{20}
    );

    EXPECT_EQ(
        bid.total_quantity,
        Quantity{20}
    );

    EXPECT_FALSE(book.best_ask().has_value());

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), Price{100});
}


TEST(OrderBookTests, BuyMatchesAsksInFifoOrder) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand first_sell =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Sell,
            Price{100},
            Quantity{20}
        );

    constexpr NewOrderCommand second_sell =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Sell,
            Price{100},
            Quantity{30}
        );

    constexpr NewOrderCommand buy =
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{12},
            Side::Buy,
            Price{100},
            Quantity{25}
        );

    ASSERT_TRUE(
        submit_order(book, first_sell, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, second_sell, OrderId{2}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, buy, OrderId{3}).has_value()
    );

    const PriceLevel& ask =
        book.ask_level(Price{100});

    ASSERT_NE(ask.front(), nullptr);

    // Order 1 should have been completely consumed first.
    EXPECT_EQ(
        ask.front()->order_id,
        OrderId{2}
    );

    // 5 units should then have been taken from order 2.
    EXPECT_EQ(
        ask.front()->remaining_quantity,
        Quantity{25}
    );

    EXPECT_EQ(
        ask.total_quantity,
        Quantity{25}
    );

    EXPECT_EQ(ask.head, ask.tail);

    EXPECT_TRUE(
        book.bid_level(Price{100}).empty()
    );
}


TEST(OrderBookTests, BuySweepsMultipleAskPriceLevels) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell_100 =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Sell,
            Price{100},
            Quantity{20}
        );

    constexpr NewOrderCommand sell_101 =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Sell,
            Price{101},
            Quantity{30}
        );

    constexpr NewOrderCommand sell_102 =
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{12},
            Side::Sell,
            Price{102},
            Quantity{40}
        );

    ASSERT_TRUE(
        submit_order(book, sell_100, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, sell_101, OrderId{2}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, sell_102, OrderId{3}).has_value()
    );

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), Price{100});

    constexpr NewOrderCommand buy =
        make_new_order(
            symbol_id,
            SequenceNumber{4},
            ClientOrderId{103},
            ClientId{13},
            Side::Buy,
            Price{102},
            Quantity{60}
        );

    ASSERT_TRUE(
        submit_order(book, buy, OrderId{4}).has_value()
    );

    // 20 @ 100 fully consumed.
    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );

    EXPECT_EQ(
        book.ask_level(Price{100}).total_quantity,
        Quantity{0}
    );

    // 30 @ 101 fully consumed.
    EXPECT_TRUE(
        book.ask_level(Price{101}).empty()
    );

    EXPECT_EQ(
        book.ask_level(Price{101}).total_quantity,
        Quantity{0}
    );

    // 10 of the 40 @ 102 consumed.
    const PriceLevel& remaining_ask =
        book.ask_level(Price{102});

    ASSERT_NE(remaining_ask.front(), nullptr);

    EXPECT_EQ(
        remaining_ask.front()->order_id,
        OrderId{3}
    );

    EXPECT_EQ(
        remaining_ask.front()->remaining_quantity,
        Quantity{30}
    );

    EXPECT_EQ(
        remaining_ask.total_quantity,
        Quantity{30}
    );

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), Price{102});

    // Incoming order was completely filled.
    EXPECT_FALSE(book.best_bid().has_value());
}


TEST(OrderBookTests, BuyStopsAtLimitPriceAndRestsRemainder) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell_100 =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Sell,
            Price{100},
            Quantity{10}
        );

    constexpr NewOrderCommand sell_102 =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Sell,
            Price{102},
            Quantity{10}
        );

    ASSERT_TRUE(
        submit_order(book, sell_100, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, sell_102, OrderId{2}).has_value()
    );

    constexpr NewOrderCommand buy =
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{12},
            Side::Buy,
            Price{101},
            Quantity{15}
        );

    ASSERT_TRUE(
        submit_order(book, buy, OrderId{3}).has_value()
    );

    // Crosses 100.
    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );

    // Must NOT cross 102 because limit is 101.
    const PriceLevel& ask_102 =
        book.ask_level(Price{102});

    ASSERT_NE(ask_102.front(), nullptr);

    EXPECT_EQ(
        ask_102.front()->remaining_quantity,
        Quantity{10}
    );

    // Remaining 5 from the buy should rest at its 101 limit.
    const PriceLevel& bid_101 =
        book.bid_level(Price{101});

    ASSERT_NE(bid_101.front(), nullptr);

    EXPECT_EQ(
        bid_101.front()->order_id,
        OrderId{3}
    );

    EXPECT_EQ(
        bid_101.front()->remaining_quantity,
        Quantity{5}
    );

    EXPECT_EQ(
        bid_101.total_quantity,
        Quantity{5}
    );

    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());

    EXPECT_EQ(*book.best_bid(), Price{101});
    EXPECT_EQ(*book.best_ask(), Price{102});
}


TEST(OrderBookTests, FullyFilledRestingAskCannotBeCancelled) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand sell =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Sell,
            Price{100},
            Quantity{50}
        );

    constexpr NewOrderCommand buy =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Buy,
            Price{100},
            Quantity{50}
        );

    ASSERT_TRUE(
        submit_order(book, sell, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, buy, OrderId{2}).has_value()
    );

    const CancelOrderCommand cancel =
        make_cancel_order(
            symbol_id,
            SequenceNumber{3},
            OrderId{1},
            ClientOrderId{100},
            ClientId{10}
        );

    const auto result =
        book.cancel(cancel);

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownOrder
    );
}

TEST(OrderBookTests, SellAboveBestBidDoesNotMatch) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        ClientId{11},
        Side::Sell,
        Price{101},
        Quantity{30}
    );

    ASSERT_TRUE(submit_order(book, buy, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, sell, OrderId{2}).has_value());

    const PriceLevel& bid =
        book.bid_level(Price{100});

    const PriceLevel& ask =
        book.ask_level(Price{101});

    ASSERT_NE(bid.front(), nullptr);
    ASSERT_NE(ask.front(), nullptr);

    EXPECT_EQ(
        bid.front()->remaining_quantity,
        Quantity{50}
    );

    EXPECT_EQ(
        ask.front()->remaining_quantity,
        Quantity{30}
    );

    EXPECT_EQ(bid.total_quantity, Quantity{50});
    EXPECT_EQ(ask.total_quantity, Quantity{30});

    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());

    EXPECT_EQ(*book.best_bid(), Price{100});
    EXPECT_EQ(*book.best_ask(), Price{101});
}


TEST(OrderBookTests, SellExactlyFillsSingleRestingBid) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        ClientId{11},
        Side::Sell,
        Price{100},
        Quantity{50}
    );

    ASSERT_TRUE(submit_order(book, buy, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, sell, OrderId{2}).has_value());

    EXPECT_TRUE(
        book.bid_level(Price{100}).empty()
    );

    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );

    EXPECT_EQ(
        book.bid_level(Price{100}).total_quantity,
        Quantity{0}
    );

    EXPECT_EQ(
        book.ask_level(Price{100}).total_quantity,
        Quantity{0}
    );

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}


TEST(OrderBookTests, SellPartiallyFillsRestingBid) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

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
        ClientId{11},
        Side::Sell,
        Price{100},
        Quantity{30}
    );

    ASSERT_TRUE(submit_order(book, buy, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, sell, OrderId{2}).has_value());

    const PriceLevel& bid =
        book.bid_level(Price{100});

    ASSERT_NE(bid.front(), nullptr);

    EXPECT_EQ(
        bid.front()->order_id,
        OrderId{1}
    );

    EXPECT_EQ(
        bid.front()->remaining_quantity,
        Quantity{20}
    );

    EXPECT_EQ(
        bid.total_quantity,
        Quantity{20}
    );

    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), Price{100});

    EXPECT_FALSE(book.best_ask().has_value());
}


TEST(OrderBookTests, SellConsumesBidAndRestsRemainder) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand buy = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        ClientId{10},
        Side::Buy,
        Price{100},
        Quantity{30}
    );

    constexpr NewOrderCommand sell = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        ClientId{11},
        Side::Sell,
        Price{100},
        Quantity{50}
    );

    ASSERT_TRUE(submit_order(book, buy, OrderId{1}).has_value());
    ASSERT_TRUE(submit_order(book, sell, OrderId{2}).has_value());

    EXPECT_TRUE(
        book.bid_level(Price{100}).empty()
    );

    const PriceLevel& ask =
        book.ask_level(Price{100});

    ASSERT_NE(ask.front(), nullptr);

    EXPECT_EQ(
        ask.front()->order_id,
        OrderId{2}
    );

    EXPECT_EQ(
        ask.front()->remaining_quantity,
        Quantity{20}
    );

    EXPECT_EQ(
        ask.total_quantity,
        Quantity{20}
    );

    EXPECT_FALSE(book.best_bid().has_value());

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), Price{100});
}


TEST(OrderBookTests, SellMatchesBidsInFifoOrder) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand first_buy =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{20}
        );

    constexpr NewOrderCommand second_buy =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Buy,
            Price{100},
            Quantity{30}
        );

    constexpr NewOrderCommand sell =
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{12},
            Side::Sell,
            Price{100},
            Quantity{25}
        );

    ASSERT_TRUE(
        submit_order(book, first_buy, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, second_buy, OrderId{2}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, sell, OrderId{3}).has_value()
    );

    const PriceLevel& bid =
        book.bid_level(Price{100});

    ASSERT_NE(bid.front(), nullptr);

    // Order 1 must be completely consumed first.
    EXPECT_EQ(
        bid.front()->order_id,
        OrderId{2}
    );

    // Then 5 units are taken from order 2.
    EXPECT_EQ(
        bid.front()->remaining_quantity,
        Quantity{25}
    );

    EXPECT_EQ(
        bid.total_quantity,
        Quantity{25}
    );

    EXPECT_EQ(bid.head, bid.tail);

    EXPECT_TRUE(
        book.ask_level(Price{100}).empty()
    );
}


TEST(OrderBookTests, SellSweepsMultipleBidPriceLevels) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand buy_102 =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{102},
            Quantity{20}
        );

    constexpr NewOrderCommand buy_101 =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Buy,
            Price{101},
            Quantity{30}
        );

    constexpr NewOrderCommand buy_100 =
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{12},
            Side::Buy,
            Price{100},
            Quantity{40}
        );

    ASSERT_TRUE(
        submit_order(book, buy_102, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, buy_101, OrderId{2}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, buy_100, OrderId{3}).has_value()
    );

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), Price{102});

    constexpr NewOrderCommand sell =
        make_new_order(
            symbol_id,
            SequenceNumber{4},
            ClientOrderId{103},
            ClientId{13},
            Side::Sell,
            Price{100},
            Quantity{60}
        );

    ASSERT_TRUE(
        submit_order(book, sell, OrderId{4}).has_value()
    );

    // 20 @ 102 consumed.
    EXPECT_TRUE(
        book.bid_level(Price{102}).empty()
    );

    // 30 @ 101 consumed.
    EXPECT_TRUE(
        book.bid_level(Price{101}).empty()
    );

    // 10 of 40 @ 100 consumed.
    const PriceLevel& remaining_bid =
        book.bid_level(Price{100});

    ASSERT_NE(remaining_bid.front(), nullptr);

    EXPECT_EQ(
        remaining_bid.front()->order_id,
        OrderId{3}
    );

    EXPECT_EQ(
        remaining_bid.front()->remaining_quantity,
        Quantity{30}
    );

    EXPECT_EQ(
        remaining_bid.total_quantity,
        Quantity{30}
    );

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), Price{100});

    // Sell was completely filled, so it must not rest.
    EXPECT_FALSE(book.best_ask().has_value());
}


TEST(OrderBookTests, SellStopsAtLimitPriceAndRestsRemainder) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand buy_102 =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{102},
            Quantity{10}
        );

    constexpr NewOrderCommand buy_100 =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Buy,
            Price{100},
            Quantity{10}
        );

    ASSERT_TRUE(
        submit_order(book, buy_102, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, buy_100, OrderId{2}).has_value()
    );

    constexpr NewOrderCommand sell =
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{102},
            ClientId{12},
            Side::Sell,
            Price{101},
            Quantity{15}
        );

    ASSERT_TRUE(
        submit_order(book, sell, OrderId{3}).has_value()
    );

    // Sell @ 101 crosses bid @ 102.
    EXPECT_TRUE(
        book.bid_level(Price{102}).empty()
    );

    // Must NOT cross bid @ 100.
    const PriceLevel& bid_100 =
        book.bid_level(Price{100});

    ASSERT_NE(bid_100.front(), nullptr);

    EXPECT_EQ(
        bid_100.front()->remaining_quantity,
        Quantity{10}
    );

    // Remaining 5 from sell rests at its own limit 101.
    const PriceLevel& ask_101 =
        book.ask_level(Price{101});

    ASSERT_NE(ask_101.front(), nullptr);

    EXPECT_EQ(
        ask_101.front()->order_id,
        OrderId{3}
    );

    EXPECT_EQ(
        ask_101.front()->remaining_quantity,
        Quantity{5}
    );

    EXPECT_EQ(
        ask_101.total_quantity,
        Quantity{5}
    );

    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());

    EXPECT_EQ(*book.best_bid(), Price{100});
    EXPECT_EQ(*book.best_ask(), Price{101});
}


TEST(OrderBookTests, FullyFilledRestingBidCannotBeCancelled) {
    constexpr SymbolId symbol_id{1};

    OrderBook book{symbol_id, test::OrderBookConfig};

    constexpr NewOrderCommand buy =
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            ClientId{10},
            Side::Buy,
            Price{100},
            Quantity{50}
        );

    constexpr NewOrderCommand sell =
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            ClientId{11},
            Side::Sell,
            Price{100},
            Quantity{50}
        );

    ASSERT_TRUE(
        submit_order(book, buy, OrderId{1}).has_value()
    );

    ASSERT_TRUE(
        submit_order(book, sell, OrderId{2}).has_value()
    );

    const CancelOrderCommand cancel =
        make_cancel_order(
            symbol_id,
            SequenceNumber{3},
            OrderId{1},
            ClientOrderId{100},
            ClientId{10}
        );

    const auto result =
        book.cancel(cancel);

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownOrder
    );
}

TEST(OrderBookBenchmarks, BuySideBookSweep) {
    using Clock = std::chrono::steady_clock;

    constexpr SymbolId symbol_id{1};

    constexpr OrderBookConfig benchmark_config{
        .max_active_orders = 512,
        .price_level_count = 1024,
        .order_lookup_capacity = 1024
    };

    constexpr std::size_t depth = 32;
    constexpr std::size_t iterations = 1'000'000;

    constexpr Price base_price{100};
    constexpr Quantity quantity_per_order{1};

    OrderBook book{
        symbol_id,
        benchmark_config
    };

    SequenceNumber sequence{1};
    ClientOrderId client_order_id{1};
    OrderId order_id{1};

    std::chrono::nanoseconds total_sweep_time{0};

    for (std::size_t iteration = 0;
         iteration < iterations;
         ++iteration) {

        //
        // Build a 128-level ask ladder:
        //
        // SELL 1 @ 100
        // SELL 1 @ 101
        // ...
        // SELL 1 @ 227
        //
        // This is deliberately OUTSIDE the timed section.
        //
        for (std::size_t level = 0;
             level < depth;
             ++level) {

            const Price price =
                base_price +
                static_cast<Price>(level);

            const NewOrderCommand sell =
                make_new_order(
                    symbol_id,
                    sequence++,
                    client_order_id++,
                    ClientId{10},
                    Side::Sell,
                    price,
                    quantity_per_order
                );

            const auto result =
                submit_order(book,
                    sell,
                    order_id++
                );

            ASSERT_TRUE(result.has_value());
        }

        ASSERT_TRUE(book.best_ask().has_value());

        EXPECT_EQ(
            *book.best_ask(),
            base_price
        );

        //
        // One enormous buy capable of consuming
        // every level.
        //
        const Price buy_limit =
            base_price +
            static_cast<Price>(depth - 1);

        const Quantity buy_quantity =
            static_cast<Quantity>(
                depth * quantity_per_order
            );

        const NewOrderCommand buy =
            make_new_order(
                symbol_id,
                sequence++,
                client_order_id++,
                ClientId{11},
                Side::Buy,
                buy_limit,
                buy_quantity
            );

        //
        // ---- TIMED REGION ----
        //
        const auto start = Clock::now();

        const auto result =
            submit_order(book,
                buy,
                order_id++
            );

        const auto end = Clock::now();
        //
        // ---- END TIMED REGION ----
        //

        ASSERT_TRUE(result.has_value());

        total_sweep_time +=
            std::chrono::duration_cast<
                std::chrono::nanoseconds
            >(end - start);

        //
        // Every iteration should completely
        // empty the book again.
        //
        ASSERT_FALSE(
            book.best_ask().has_value()
        );

        ASSERT_FALSE(
            book.best_bid().has_value()
        );
    }

    const std::uint64_t executions =
        static_cast<std::uint64_t>(
            depth * iterations
        );

    const double seconds =
        std::chrono::duration<double>(
            total_sweep_time
        ).count();

    const double executions_per_second =
        static_cast<double>(executions) /
        seconds;

    const double ns_per_execution =
        static_cast<double>(
            total_sweep_time.count()
        ) /
        static_cast<double>(executions);

    const double ns_per_sweep =
        static_cast<double>(
            total_sweep_time.count()
        ) /
        static_cast<double>(iterations);

    std::cerr
        << "\n"
        << "=== Buy-side sweep benchmark ===\n"
        << "depth:              "
        << depth << '\n'
        << "iterations:         "
        << iterations << '\n'
        << "executions:         "
        << executions << '\n'
        << "measured time:      "
        << seconds << " s\n"
        << "executions/sec:     "
        << executions_per_second << '\n'
        << "ns/execution:       "
        << ns_per_execution << '\n'
        << "ns/sweep:           "
        << ns_per_sweep << '\n';
}

} // namespace
} // namespace sixchange
