#include <gtest/gtest.h>

#include <memory>

#include <engine/OrderBook.h>

namespace sixchange {
    TEST(OrderBookTest, RestsBuyOrderAtCorrectPrice)
    {
        constexpr SymbolId symbol_id{1};

        OrderBook book{symbol_id};

        constexpr NewOrderCommand command{
            .seq = 1,
            .price = 100,
            .quantity = 50,
            .client_order_id = 100,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Buy,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        };

        book.add(command, 1);

        const PriceLevel& level = book.bid_level(100);

        ASSERT_FALSE(level.empty());
        ASSERT_NE(level.front(), nullptr);

        EXPECT_EQ(level.total_quantity, 50);
        EXPECT_EQ(level.front()->order_id, 1);
        EXPECT_EQ(level.front()->price, 100);
        EXPECT_EQ(level.front()->remaining_quantity, 50);
        EXPECT_EQ(level.front()->side, Side::Buy);
    }

    TEST(OrderBookTest, RestsSellOrderAtCorrectPrice)
    {
        constexpr SymbolId symbol_id{1};

        OrderBook book{symbol_id};

        constexpr NewOrderCommand command{
            .seq = 1,
            .price = 105,
            .quantity = 25,
            .client_order_id = 100,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Sell,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        };

        book.add(command, 1);

        const PriceLevel& level = book.ask_level(105);

        ASSERT_FALSE(level.empty());
        ASSERT_NE(level.front(), nullptr);

        EXPECT_EQ(level.total_quantity, 25);
        EXPECT_EQ(level.front()->side, Side::Sell);
    }

    TEST(OrderBookTest, PreservesFifoWithinPriceLevel)
    {
        constexpr SymbolId symbol_id{1};

        OrderBook book{symbol_id};

        constexpr NewOrderCommand first{
            .seq = 1,
            .price = 100,
            .quantity = 50,
            .client_order_id = 100,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Buy,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        };

        constexpr NewOrderCommand second{
            .seq = 2,
            .price = 100,
            .quantity = 20,
            .client_order_id = 101,
            .client_id = 11,
            .symbol_id = symbol_id,
            .side = Side::Buy,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        };

        book.add(first, 1);
        book.add(second, 2);

        const PriceLevel& level = book.bid_level(100);

        ASSERT_NE(level.head, nullptr);
        ASSERT_NE(level.tail, nullptr);

        EXPECT_EQ(level.head->order_id, 1);
        EXPECT_EQ(level.tail->order_id, 2);

        ASSERT_NE(level.head->next, nullptr);

        EXPECT_EQ(level.head->next, level.tail);
        EXPECT_EQ(level.tail->prev, level.head);

        EXPECT_EQ(level.total_quantity, 70);
    }

} // namespace sixchange
