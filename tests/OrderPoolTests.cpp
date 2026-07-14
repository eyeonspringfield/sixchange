#include <gtest/gtest.h>

#include <engine/OrderPool.h>

namespace sixchange {

TEST(OrderPoolTest, AllocatesOrderWithinCapacity)
{
    OrderPool<1> pool;

    Order* order = pool.allocate();

    ASSERT_NE(order, nullptr);
}

TEST(OrderPoolTest, ReturnsNullWhenCapacityIsExhausted)
{
    OrderPool<2> pool;

    Order* first = pool.allocate();
    Order* second = pool.allocate();
    Order* third = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(third, nullptr);
}

TEST(OrderPoolTest, AllocatesDistinctOrders)
{
    OrderPool<2> pool;

    Order* first = pool.allocate();
    Order* second = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_NE(first, second);
}

TEST(OrderPoolTest, NewlyAllocatedOrderIsValueInitialized)
{
    OrderPool<1> pool;

    Order* order = pool.allocate();

    ASSERT_NE(order, nullptr);

    EXPECT_EQ(order->order_id, OrderId{});
    EXPECT_EQ(order->client_order_id, ClientOrderId{});
    EXPECT_EQ(order->client_id, ClientId{});
    EXPECT_EQ(order->symbol_id, SymbolId{});
    EXPECT_EQ(order->price, Price{});
    EXPECT_EQ(order->remaining_quantity, Quantity{});
    EXPECT_EQ(order->seq, SequenceNumber{});
    EXPECT_EQ(order->prev, nullptr);
    EXPECT_EQ(order->next, nullptr);
    EXPECT_FALSE(order->active);
}

TEST(OrderPoolTest, ReusesReleasedOrder)
{
    OrderPool<1> pool;

    Order* first = pool.allocate();

    ASSERT_NE(first, nullptr);

    pool.release(first);

    Order* second = pool.allocate();

    ASSERT_NE(second, nullptr);

    EXPECT_EQ(second, first);
}

TEST(OrderPoolTest, ResetsOrderWhenReleasedAndReallocated)
{
    OrderPool<1> pool;

    Order* order = pool.allocate();

    ASSERT_NE(order, nullptr);

    order->order_id = 42;
    order->client_order_id = 100;
    order->client_id = 10;
    order->symbol_id = 2;
    order->price = 123;
    order->remaining_quantity = 50;
    order->seq = 99;
    order->active = true;

    pool.release(order);

    Order* reused = pool.allocate();

    ASSERT_NE(reused, nullptr);

    EXPECT_EQ(reused->order_id, OrderId{});
    EXPECT_EQ(reused->client_order_id, ClientOrderId{});
    EXPECT_EQ(reused->client_id, ClientId{});
    EXPECT_EQ(reused->symbol_id, SymbolId{});
    EXPECT_EQ(reused->price, Price{});
    EXPECT_EQ(reused->remaining_quantity, Quantity{});
    EXPECT_EQ(reused->seq, SequenceNumber{});
    EXPECT_EQ(reused->prev, nullptr);
    EXPECT_EQ(reused->next, nullptr);
    EXPECT_FALSE(reused->active);
}

TEST(OrderPoolTest, ReleasedOrderBecomesAvailableAfterCapacityExhaustion)
{
    OrderPool<2> pool;

    Order* first = pool.allocate();
    Order* second = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_EQ(pool.allocate(), nullptr);

    pool.release(first);

    Order* reused = pool.allocate();

    ASSERT_NE(reused, nullptr);

    EXPECT_EQ(reused, first);
}

TEST(OrderPoolTest, ReusesReleasedOrdersInLifoOrder)
{
    OrderPool<3> pool;

    Order* first = pool.allocate();
    Order* second = pool.allocate();
    Order* third = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);

    pool.release(first);
    pool.release(second);

    Order* reused_first = pool.allocate();
    Order* reused_second = pool.allocate();

    EXPECT_EQ(reused_first, second);
    EXPECT_EQ(reused_second, first);
}

TEST(OrderPoolTest, ReusedOrderDoesNotReduceTotalPoolCapacity)
{
    OrderPool<2> pool;

    Order* first = pool.allocate();
    Order* second = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    pool.release(first);

    Order* reused = pool.allocate();

    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused, first);

    EXPECT_EQ(pool.allocate(), nullptr);
}

} // namespace sixchange