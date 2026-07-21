#include <gtest/gtest.h>

#include <engine/PriceLevel.h>

namespace sixchange {

TEST(PriceLevelTest, IsEmptyByDefault) {
    constexpr PriceLevel level{};

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    EXPECT_EQ(level.total_quantity, Quantity{});
    EXPECT_EQ(level.front(), nullptr);
}

TEST(PriceLevelTest, PushBackAddsFirstOrder) {
    PriceLevel level{};

    Order order{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    level.push_back(&order);

    EXPECT_FALSE(level.empty());

    EXPECT_EQ(level.head, &order);
    EXPECT_EQ(level.tail, &order);
    EXPECT_EQ(level.front(), &order);

    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);

    EXPECT_EQ(level.total_quantity, 10);
}

TEST(PriceLevelTest, PushBackPreservesFifoOrder) {
    PriceLevel level{};

    Order first{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    Order second{
        .order_id = 2,
        .remaining_quantity = 20,
    };

    Order third{
        .order_id = 3,
        .remaining_quantity = 30,
    };

    level.push_back(&first);
    level.push_back(&second);
    level.push_back(&third);

    EXPECT_EQ(level.head, &first);
    EXPECT_EQ(level.tail, &third);
    EXPECT_EQ(level.front(), &first);

    EXPECT_EQ(first.prev, nullptr);
    EXPECT_EQ(first.next, &second);

    EXPECT_EQ(second.prev, &first);
    EXPECT_EQ(second.next, &third);

    EXPECT_EQ(third.prev, &second);
    EXPECT_EQ(third.next, nullptr);

    EXPECT_EQ(level.total_quantity, 60);
}

TEST(PriceLevelTest, RemoveOnlyOrderEmptiesLevel) {
    PriceLevel level{};

    Order order{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    level.push_back(&order);
    level.remove(&order);

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    EXPECT_EQ(level.front(), nullptr);
    EXPECT_EQ(level.total_quantity, 0);

    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);
}

TEST(PriceLevelTest, RemoveHeadUpdatesHead) {
    PriceLevel level{};

    Order first{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    Order second{
        .order_id = 2,
        .remaining_quantity = 20,
    };

    level.push_back(&first);
    level.push_back(&second);

    level.remove(&first);

    EXPECT_EQ(level.head, &second);
    EXPECT_EQ(level.tail, &second);
    EXPECT_EQ(level.front(), &second);

    EXPECT_EQ(second.prev, nullptr);
    EXPECT_EQ(second.next, nullptr);

    EXPECT_EQ(first.prev, nullptr);
    EXPECT_EQ(first.next, nullptr);

    EXPECT_EQ(level.total_quantity, 20);
}

TEST(PriceLevelTest, RemoveTailUpdatesTail) {
    PriceLevel level{};

    Order first{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    Order second{
        .order_id = 2,
        .remaining_quantity = 20,
    };

    level.push_back(&first);
    level.push_back(&second);

    level.remove(&second);

    EXPECT_EQ(level.head, &first);
    EXPECT_EQ(level.tail, &first);
    EXPECT_EQ(level.front(), &first);

    EXPECT_EQ(first.prev, nullptr);
    EXPECT_EQ(first.next, nullptr);

    EXPECT_EQ(second.prev, nullptr);
    EXPECT_EQ(second.next, nullptr);

    EXPECT_EQ(level.total_quantity, 10);
}

TEST(PriceLevelTest, RemoveMiddleOrderReconnectsNeighbors) {
    PriceLevel level{};

    Order first{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    Order second{
        .order_id = 2,
        .remaining_quantity = 20,
    };

    Order third{
        .order_id = 3,
        .remaining_quantity = 30,
    };

    level.push_back(&first);
    level.push_back(&second);
    level.push_back(&third);

    level.remove(&second);

    EXPECT_EQ(level.head, &first);
    EXPECT_EQ(level.tail, &third);

    EXPECT_EQ(first.prev, nullptr);
    EXPECT_EQ(first.next, &third);

    EXPECT_EQ(third.prev, &first);
    EXPECT_EQ(third.next, nullptr);

    EXPECT_EQ(second.prev, nullptr);
    EXPECT_EQ(second.next, nullptr);

    EXPECT_EQ(level.total_quantity, 40);
}

TEST(PriceLevelTest, RemoveSubtractsRemainingQuantity) {
    PriceLevel level{};

    Order first{
        .remaining_quantity = 100,
    };

    Order second{
        .remaining_quantity = 50,
    };

    level.push_back(&first);
    level.push_back(&second);

    ASSERT_EQ(level.total_quantity, 150);

    level.remove(&first);

    EXPECT_EQ(level.total_quantity, 50);
}

TEST(PriceLevelTest, CanAppendAfterRemovingTail) {
    PriceLevel level{};

    Order first{
        .order_id = 1,
        .remaining_quantity = 10,
    };

    Order second{
        .order_id = 2,
        .remaining_quantity = 20,
    };

    Order third{
        .order_id = 3,
        .remaining_quantity = 30,
    };

    level.push_back(&first);
    level.push_back(&second);

    level.remove(&second);
    level.push_back(&third);

    EXPECT_EQ(level.head, &first);
    EXPECT_EQ(level.tail, &third);

    EXPECT_EQ(first.next, &third);
    EXPECT_EQ(third.prev, &first);
    EXPECT_EQ(third.next, nullptr);

    EXPECT_EQ(level.total_quantity, 40);
}

} // namespace sixchange
