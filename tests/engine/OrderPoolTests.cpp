#include <gtest/gtest.h>

#include "engine/OrderPool.h"

namespace sixchange {

TEST(OrderPoolTests, ReportsRuntimeCapacity)
{
    const OrderPool pool{3};

    EXPECT_EQ(pool.capacity(), std::size_t{3});
    EXPECT_EQ(
        pool.allocated_slots(),
        std::size_t{0}
    );
}

TEST(
    OrderPoolTests,
    AllocatesOrderWithinCapacity)
{
    OrderPool pool{1};

    Order* const order = pool.allocate();

    ASSERT_NE(order, nullptr);
    EXPECT_EQ(
        pool.allocated_slots(),
        std::size_t{1}
    );
}

TEST(
    OrderPoolTests,
    ReturnsNullWhenCapacityIsExhausted)
{
    OrderPool pool{2};

    Order* const first = pool.allocate();
    Order* const second = pool.allocate();
    Order* const third = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(third, nullptr);
    EXPECT_EQ(
        pool.allocated_slots(),
        std::size_t{2}
    );
}

TEST(OrderPoolTests, AllocatesDistinctOrders)
{
    OrderPool pool{2};

    Order* const first = pool.allocate();
    Order* const second = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
}

TEST(
    OrderPoolTests,
    NewlyAllocatedOrderIsValueInitialized)
{
    OrderPool pool{1};

    Order* const order = pool.allocate();

    ASSERT_NE(order, nullptr);

    EXPECT_EQ(order->order_id, OrderId{});
    EXPECT_EQ(
        order->client_order_id,
        ClientOrderId{}
    );
    EXPECT_EQ(order->client_id, ClientId{});
    EXPECT_EQ(order->symbol_id, SymbolId{});
    EXPECT_EQ(order->price, Price{});
    EXPECT_EQ(
        order->remaining_quantity,
        Quantity{}
    );
    EXPECT_EQ(order->seq, SequenceNumber{});
    EXPECT_EQ(order->prev, nullptr);
    EXPECT_EQ(order->next, nullptr);
    EXPECT_FALSE(order->active);
}

TEST(OrderPoolTests, ReusesReleasedOrder)
{
    OrderPool pool{1};

    Order* const first = pool.allocate();

    ASSERT_NE(first, nullptr);

    pool.release(first);

    Order* const second = pool.allocate();

    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second, first);
}

TEST(
    OrderPoolTests,
    ResetsOrderWhenReleasedAndReallocated)
{
    OrderPool pool{1};

    Order* const order = pool.allocate();

    ASSERT_NE(order, nullptr);

    order->order_id = OrderId{42};
    order->client_order_id =
        ClientOrderId{100};
    order->client_id = ClientId{10};
    order->symbol_id = SymbolId{2};
    order->price = Price{123};
    order->remaining_quantity =
        Quantity{50};
    order->seq = SequenceNumber{99};
    order->active = true;

    pool.release(order);

    Order* const reused = pool.allocate();

    ASSERT_NE(reused, nullptr);

    EXPECT_EQ(reused->order_id, OrderId{});
    EXPECT_EQ(
        reused->client_order_id,
        ClientOrderId{}
    );
    EXPECT_EQ(
        reused->client_id,
        ClientId{}
    );
    EXPECT_EQ(
        reused->symbol_id,
        SymbolId{}
    );
    EXPECT_EQ(reused->price, Price{});
    EXPECT_EQ(
        reused->remaining_quantity,
        Quantity{}
    );
    EXPECT_EQ(reused->seq, SequenceNumber{});
    EXPECT_EQ(reused->prev, nullptr);
    EXPECT_EQ(reused->next, nullptr);
    EXPECT_FALSE(reused->active);
}

TEST(
    OrderPoolTests,
    ReleasedOrderBecomesAvailableAfterExhaustion)
{
    OrderPool pool{2};

    Order* const first = pool.allocate();
    Order* const second = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(pool.allocate(), nullptr);

    pool.release(first);

    Order* const reused = pool.allocate();

    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused, first);
}

TEST(
    OrderPoolTests,
    ReusesReleasedOrdersInLifoOrder)
{
    OrderPool pool{3};

    Order* const first = pool.allocate();
    Order* const second = pool.allocate();
    Order* const third = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);

    pool.release(first);
    pool.release(second);

    Order* const reused_first =
        pool.allocate();

    Order* const reused_second =
        pool.allocate();

    EXPECT_EQ(reused_first, second);
    EXPECT_EQ(reused_second, first);
}

TEST(
    OrderPoolTests,
    AllocatedSlotCountIsHighWaterMark)
{
    OrderPool pool{2};

    Order* const first = pool.allocate();
    Order* const second = pool.allocate();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_EQ(
        pool.allocated_slots(),
        std::size_t{2}
    );

    pool.release(first);

    EXPECT_EQ(
        pool.allocated_slots(),
        std::size_t{2}
    );

    EXPECT_EQ(pool.allocate(), first);

    EXPECT_EQ(
        pool.allocated_slots(),
        std::size_t{2}
    );
}

} // namespace sixchange