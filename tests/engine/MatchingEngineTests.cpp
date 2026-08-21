#include <gtest/gtest.h>

#include <sixchange/core/Commands.h>

#include "engine/MatchingEngine.h"
#include "TestConfig.h"
#include "TestSinks.h"

namespace sixchange {
namespace {

[[nodiscard]] constexpr EngineCommand make_new_order(
    const SymbolId symbol_id,
    const SequenceNumber sequence_number,
    const ClientOrderId client_order_id,
    const Side side,
    const Price price,
    const Quantity quantity) {
    return EngineCommand{
        .type = CommandType::NewOrder,
        .new_order = {
            .seq = sequence_number,
            .price = price,
            .quantity = quantity,
            .client_order_id = client_order_id,
            .client_id = ClientId{10},
            .symbol_id = symbol_id,
            .side = side,
            .order_type = OrderType::Limit,
            .tif = TimeInForce::GFD
        }
    };
}

[[nodiscard]]
constexpr EngineCommand make_cancel_order(
    const SymbolId symbol_id,
    const SequenceNumber sequence_number,
    const OrderId order_id,
    const ClientOrderId client_order_id,
    const ClientId client_id = ClientId{10})
{
    return EngineCommand{
        .type = CommandType::CancelOrder,
        .cancel_order = {
            .seq = sequence_number,
            .order_id = order_id,
            .client_order_id = client_order_id,
            .client_id = client_id,
            .symbol_id = symbol_id
        }
    };
}

TEST(MatchingEngineTests, ProcessesNewBuyOrder) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    constexpr EngineCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Buy,
        Price{100},
        Quantity{50}
    );
    const auto result = engine.process(command);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, OrderId{1});

    const PriceLevel& level =
        engine.order_book().bid_level(Price{100});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.front(), nullptr);

    EXPECT_EQ(level.total_quantity, Quantity{50});
    EXPECT_EQ(level.front()->order_id, OrderId{1});
    EXPECT_EQ(
        level.front()->client_order_id,
        ClientOrderId{100}
    );
    EXPECT_EQ(level.front()->client_id, ClientId{10});
    EXPECT_EQ(level.front()->symbol_id, symbol_id);
    EXPECT_EQ(level.front()->seq, SequenceNumber{1});
    EXPECT_EQ(level.front()->side, Side::Buy);
    EXPECT_EQ(level.front()->price, Price{100});
    EXPECT_EQ(
        level.front()->remaining_quantity,
        Quantity{50}
    );
    EXPECT_TRUE(level.front()->active);
}

TEST(MatchingEngineTests, ProcessesNewSellOrder) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    constexpr EngineCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Sell,
        Price{105},
        Quantity{25}
    );
    const auto result = engine.process(command);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, OrderId{1});

    const PriceLevel& level =
        engine.order_book().ask_level(Price{105});

    ASSERT_FALSE(level.empty());
    ASSERT_NE(level.front(), nullptr);

    EXPECT_EQ(level.total_quantity, Quantity{25});
    EXPECT_EQ(level.front()->order_id, OrderId{1});
    EXPECT_EQ(
        level.front()->client_order_id,
        ClientOrderId{100}
    );
    EXPECT_EQ(level.front()->client_id, ClientId{10});
    EXPECT_EQ(level.front()->symbol_id, symbol_id);
    EXPECT_EQ(level.front()->seq, SequenceNumber{1});
    EXPECT_EQ(level.front()->side, Side::Sell);
    EXPECT_EQ(level.front()->price, Price{105});
    EXPECT_EQ(
        level.front()->remaining_quantity,
        Quantity{25}
    );
    EXPECT_TRUE(level.front()->active);
}

TEST(MatchingEngineTests, AssignsIncreasingOrderIds) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    constexpr EngineCommand first = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Buy,
        Price{100},
        Quantity{10}
    );

    constexpr EngineCommand second = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        Side::Buy,
        Price{101},
        Quantity{20}
    );
    const auto first_result = engine.process(first);
    const auto second_result = engine.process(second);

    ASSERT_TRUE(first_result.has_value());
    ASSERT_TRUE(second_result.has_value());

    EXPECT_EQ(*first_result, OrderId{1});
    EXPECT_EQ(*second_result, OrderId{2});

    const PriceLevel& first_level =
        engine.order_book().bid_level(Price{100});

    const PriceLevel& second_level =
        engine.order_book().bid_level(Price{101});

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
}

TEST(MatchingEngineTests, RejectsOrderForWrongSymbol) {
    constexpr SymbolId engine_symbol{1};

    test::MatchingEngineHarness engine{engine_symbol, test::OrderBookConfig};

    constexpr EngineCommand command = make_new_order(
        SymbolId{2},
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Buy,
        Price{100},
        Quantity{10}
    );
    const auto result = engine.process(command);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownSymbol
    );

    const PriceLevel& level =
        engine.order_book().bid_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, Quantity{0});
}

TEST(MatchingEngineTests, RejectsOrderWithZeroQuantity) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id};

    constexpr EngineCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Buy,
        Price{100},
        Quantity{0}
    );
    const auto result = engine.process(command);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::InvalidQuantity
    );

    const PriceLevel& level =
        engine.order_book().bid_level(Price{100});

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, Quantity{0});
}

TEST(MatchingEngineTests, RejectsOrderWithOutOfRangePrice) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    constexpr EngineCommand command = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Buy,
        Price{test::OrderBookConfig.price_level_count + 1},
        Quantity{10}
    );
    const auto result = engine.process(command);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        RejectReason::InvalidPrice
    );
}

TEST(MatchingEngineTests, RejectedOrderDoesNotConsumeOrderId) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id};

    constexpr EngineCommand rejected = make_new_order(
        symbol_id,
        SequenceNumber{1},
        ClientOrderId{100},
        Side::Buy,
        Price{100},
        Quantity{0}
    );

    constexpr EngineCommand accepted = make_new_order(
        symbol_id,
        SequenceNumber{2},
        ClientOrderId{101},
        Side::Buy,
        Price{101},
        Quantity{10}
    );
    const auto rejected_result =
        engine.process(rejected);

    const auto accepted_result =
        engine.process(accepted);

    ASSERT_FALSE(rejected_result.has_value());
    ASSERT_TRUE(accepted_result.has_value());

    EXPECT_EQ(*accepted_result, OrderId{1});

    const PriceLevel& level =
        engine.order_book().bid_level(Price{101});

    ASSERT_NE(level.front(), nullptr);
    EXPECT_EQ(level.front()->order_id, OrderId{1});
}

TEST(MatchingEngineTests, PreservesCommandSequenceNumber) {
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{symbol_id};

    constexpr EngineCommand command = make_new_order(
        symbol_id,
        SequenceNumber{42},
        ClientOrderId{100},
        Side::Buy,
        Price{100},
        Quantity{10}
    );
    const auto result = engine.process(command);

    ASSERT_TRUE(result.has_value());

    const PriceLevel& level =
        engine.order_book().bid_level(Price{100});

    ASSERT_NE(level.front(), nullptr);
    EXPECT_EQ(
        level.front()->seq,
        SequenceNumber{42}
    );
}

TEST(
    MatchingEngineTests,
    ProcessesCancellation)
{
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{
        symbol_id,
        test::OrderBookConfig
    };
    const auto accepted = engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            Side::Buy,
            Price{100},
            Quantity{10}
        ));

    ASSERT_TRUE(accepted.has_value());
    EXPECT_EQ(*accepted, OrderId{1});

    const auto cancelled = engine.process(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100}
        ));

    ASSERT_TRUE(cancelled.has_value());
    EXPECT_EQ(*cancelled, OrderId{1});

    EXPECT_TRUE(
        engine.order_book()
            .bid_level(Price{100})
            .empty()
    );
}

TEST(
    MatchingEngineTests,
    PropagatesUnknownOrderCancellation)
{
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{
        symbol_id,
        test::OrderBookConfig
    };
    const auto result = engine.process(
        make_cancel_order(
            symbol_id,
            SequenceNumber{1},
            OrderId{999},
            ClientOrderId{100}
        ));

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::UnknownOrder
    );
}

TEST(
    MatchingEngineTests,
    PropagatesCancellationOwnershipFailure)
{
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{
        symbol_id,
        test::OrderBookConfig
    };
    ASSERT_TRUE(engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            Side::Buy,
            Price{100},
            Quantity{10}
        )).has_value());

    const auto result = engine.process(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100},
            ClientId{11}
        ));

    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(
        result.error(),
        RejectReason::NotOrderOwner
    );
}

TEST(
    MatchingEngineTests,
    CancellationDoesNotConsumeOrderId)
{
    constexpr SymbolId symbol_id{1};

    test::MatchingEngineHarness engine{
        symbol_id,
        test::OrderBookConfig
    };
    const auto first = engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{1},
            ClientOrderId{100},
            Side::Buy,
            Price{100},
            Quantity{10}
        ));

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, OrderId{1});

    ASSERT_TRUE(engine.process(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100}
        )).has_value());

    const auto second = engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{3},
            ClientOrderId{101},
            Side::Buy,
            Price{101},
            Quantity{10}
        ));

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, OrderId{2});
}


TEST(MatchingEngineEventTests, EmitsAcceptedThenRested) {
    constexpr SymbolId symbol_id{1};
    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    const auto result = engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{7},
            ClientOrderId{100},
            Side::Buy,
            Price{100},
            Quantity{25}
        )
    );

    ASSERT_TRUE(result.has_value());

    const auto& events = engine.events();
    ASSERT_EQ(events.size(), 2U);

    ASSERT_EQ(events[0].type, EngineEventType::OrderAccepted);
    EXPECT_EQ(events[0].order_accepted.seq, SequenceNumber{7});
    EXPECT_EQ(events[0].order_accepted.order_id, OrderId{1});
    EXPECT_EQ(events[0].order_accepted.client_order_id, ClientOrderId{100});
    EXPECT_EQ(events[0].order_accepted.quantity, Quantity{25});

    ASSERT_EQ(events[1].type, EngineEventType::OrderRested);
    EXPECT_EQ(events[1].order_rested.seq, SequenceNumber{7});
    EXPECT_EQ(events[1].order_rested.order_id, OrderId{1});
    EXPECT_EQ(events[1].order_rested.remaining_quantity, Quantity{25});
}

TEST(MatchingEngineEventTests, EmitsTradeWithEngineTradeId) {
    constexpr SymbolId symbol_id{1};
    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    ASSERT_TRUE(
        engine.process(
            make_new_order(
                symbol_id,
                SequenceNumber{1},
                ClientOrderId{100},
                Side::Sell,
                Price{100},
                Quantity{10}
            )
        ).has_value()
    );

    const auto result = engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{2},
            ClientOrderId{101},
            Side::Buy,
            Price{100},
            Quantity{10}
        )
    );

    ASSERT_TRUE(result.has_value());

    const auto& events = engine.events();
    ASSERT_EQ(events.size(), 2U);
    ASSERT_EQ(events[0].type, EngineEventType::OrderAccepted);
    ASSERT_EQ(events[1].type, EngineEventType::TradeExecuted);

    const TradeExecutedEvent& trade = events[1].trade_executed;
    EXPECT_EQ(trade.seq, SequenceNumber{2});
    EXPECT_EQ(trade.trade_id, TradeId{1});
    EXPECT_EQ(trade.price, Price{100});
    EXPECT_EQ(trade.quantity, Quantity{10});
    EXPECT_EQ(trade.aggressing_side, Side::Buy);
    EXPECT_EQ(trade.aggressing_order_id, OrderId{2});
    EXPECT_EQ(trade.resting_order_id, OrderId{1});
    EXPECT_EQ(trade.aggressing_remaining_quantity, Quantity{0});
    EXPECT_EQ(trade.resting_remaining_quantity, Quantity{0});
}

TEST(MatchingEngineEventTests, EmitsCommandRejected) {
    constexpr SymbolId symbol_id{1};
    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    const auto result = engine.process(
        make_new_order(
            symbol_id,
            SequenceNumber{9},
            ClientOrderId{100},
            Side::Buy,
            Price{100},
            Quantity{0}
        )
    );

    ASSERT_FALSE(result.has_value());

    const auto& events = engine.events();
    ASSERT_EQ(events.size(), 1U);
    ASSERT_EQ(events[0].type, EngineEventType::CommandRejected);
    EXPECT_EQ(events[0].command_rejected.seq, SequenceNumber{9});
    EXPECT_EQ(events[0].command_rejected.command_type, CommandType::NewOrder);
    EXPECT_EQ(events[0].command_rejected.reason, RejectReason::InvalidQuantity);
}

TEST(MatchingEngineEventTests, EmitsOrderCancelled) {
    constexpr SymbolId symbol_id{1};
    test::MatchingEngineHarness engine{symbol_id, test::OrderBookConfig};

    ASSERT_TRUE(
        engine.process(
            make_new_order(
                symbol_id,
                SequenceNumber{1},
                ClientOrderId{100},
                Side::Buy,
                Price{100},
                Quantity{10}
            )
        ).has_value()
    );

    const auto result = engine.process(
        make_cancel_order(
            symbol_id,
            SequenceNumber{2},
            OrderId{1},
            ClientOrderId{100}
        )
    );

    ASSERT_TRUE(result.has_value());

    const auto& events = engine.events();
    ASSERT_EQ(events.size(), 1U);
    ASSERT_EQ(events[0].type, EngineEventType::OrderCancelled);
    EXPECT_EQ(events[0].order_cancelled.seq, SequenceNumber{2});
    EXPECT_EQ(events[0].order_cancelled.order_id, OrderId{1});
    EXPECT_EQ(events[0].order_cancelled.cancelled_quantity, Quantity{10});
}

} // namespace
} // namespace sixchange
