#include <gtest/gtest.h>

#include <sixchange/core/Commands.h>

#include <engine/MatchingEngine.h>

namespace sixchange {

TEST(MatchingEngineTest, ProcessesNewBuyOrder)
{
    constexpr SymbolId symbol_id{1};

    MatchingEngine engine{symbol_id};
    EventBuffer<OrderBookMaxEventsPerCommand> events;

    constexpr EngineCommand command{
        .type = CommandType::NewOrder,
        .new_order = {
            .seq = 1,
            .price = 100,
            .quantity = 50,
            .client_order_id = 100,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Buy,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        }
    };

    engine.process(command, events);

    const PriceLevel& level =
        engine.order_book().bid_level(100);

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.front(), nullptr);

    EXPECT_EQ(level.total_quantity, 50);
    EXPECT_EQ(level.front()->order_id, 1);
    EXPECT_EQ(level.front()->side, Side::Buy);
    EXPECT_EQ(level.front()->price, 100);
}

TEST(MatchingEngineTest, ProcessesNewSellOrder)
{
    constexpr SymbolId symbol_id{1};

    MatchingEngine engine{symbol_id};
    EventBuffer<OrderBookMaxEventsPerCommand> events;

    constexpr EngineCommand command{
        .type = CommandType::NewOrder,
        .new_order = {
            .seq = 1,
            .price = 105,
            .quantity = 25,
            .client_order_id = 100,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Sell,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        }
    };

    engine.process(command, events);

    const PriceLevel& level =
        engine.order_book().ask_level(105);

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.front(), nullptr);

    EXPECT_EQ(level.total_quantity, 25);
    EXPECT_EQ(level.front()->order_id, 1);
    EXPECT_EQ(level.front()->side, Side::Sell);
    EXPECT_EQ(level.front()->price, 105);
}

TEST(MatchingEngineTest, AssignsIncreasingOrderIds)
{
    constexpr SymbolId symbol_id{1};

    MatchingEngine engine{symbol_id};
    EventBuffer<OrderBookMaxEventsPerCommand> events;

    constexpr EngineCommand first{
        .type = CommandType::NewOrder,
        .new_order = {
            .seq = 1,
            .price = 100,
            .quantity = 10,
            .client_order_id = 100,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Buy,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        }
    };

    constexpr EngineCommand second{
        .type = CommandType::NewOrder,
        .new_order = {
            .seq = 2,
            .price = 101,
            .quantity = 20,
            .client_order_id = 101,
            .client_id = 10,
            .symbol_id = symbol_id,
            .side = Side::Buy,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD,
        }
    };

    engine.process(first, events);
    engine.process(second, events);

    const PriceLevel& first_level =
        engine.order_book().bid_level(100);

    const PriceLevel& second_level =
        engine.order_book().bid_level(101);

    ASSERT_NE(first_level.front(), nullptr);
    ASSERT_NE(second_level.front(), nullptr);

    EXPECT_EQ(first_level.front()->order_id, 1);
    EXPECT_EQ(second_level.front()->order_id, 2);
}

} // namespace sixchange
