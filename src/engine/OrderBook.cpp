#include "OrderBook.h"

#include <cassert>
#include <cstddef>
#include <expected>
#include <stdexcept>

#include "logging/Log.h"
#include "core/EnumNames.h"

namespace sixchange {

OrderBook::OrderBook(
    const SymbolId symbol_id,
    const OrderBookConfig& config)
    : symbol_id_{symbol_id},
      config_{config},
      orders_{config.max_active_orders},
      orders_by_id_{config.order_lookup_capacity},
      bids_{std::make_unique<PriceLevel[]>(config.price_level_count)},
      asks_{std::make_unique<PriceLevel[]>(config.price_level_count)}
{
    if (config.max_active_orders == 0 || config.price_level_count == 0
        || config.order_lookup_capacity < config.max_active_orders
        || (config.order_lookup_capacity & (config.order_lookup_capacity - 1)) != 0) {
        throw std::invalid_argument{"Invalid order book configuration"};
    }

    for (std::size_t index{0}; index < config_.price_level_count; ++index) {
        bids_[index].price = index;
        asks_[index].price =index;
    }
}

OrderBook::AdmitResult OrderBook::admit(const NewOrderCommand& command, OrderId order_id) noexcept {
    if (command.symbol_id != symbol_id_) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected sequence={}, order_id={}, reason={}, command_symbol_id={}, book_symbol_id={}",
            command.seq,
            order_id,
            names::reject_reason(RejectReason::UnknownSymbol),
            command.symbol_id,
            symbol_id_
        );

        return std::unexpected{RejectReason::UnknownSymbol};
    }

    if (command.quantity == Quantity{0}) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected sequence={}, order_id={}, reason={}",
            command.seq,
            order_id,
            names::reject_reason(RejectReason::InvalidQuantity)
        );

        return std::unexpected{RejectReason::InvalidQuantity};
    }

    if (command.price >= config_.price_level_count) {
        SIXCHANGE_LOG_DEBUG(
            "New order rejected sequence={}, order_id={}, price={}, price_level_count={}, reason={}",
            command.seq,
            order_id,
            command.price,
            config_.price_level_count,
            names::reject_reason(RejectReason::InvalidPrice)
        );

        return std::unexpected{RejectReason::InvalidPrice};
    }

    Order* order = orders_.allocate();
    if (order == nullptr) {
        SIXCHANGE_LOG_WARNING(
            "Order pool exhausted sequence={}, order_id={}, capacity={}",
            command.seq,
            order_id,
            orders_.capacity()
        );

        return std::unexpected{RejectReason::CapacityExhausted};
    }

    order->order_id = order_id;
    order->client_order_id = command.client_order_id;
    order->client_id = command.client_id;
    order->symbol_id = command.symbol_id;
    order->side = command.side;
    order->price = command.price;
    order->remaining_quantity = command.quantity;
    order->seq = command.seq;
    order->active = true;

    switch (orders_by_id_.insert(order_id, order)) {
    case FixedOrderMap<OrderId, Order*>::InsertResult::Inserted:
        break;

    case FixedOrderMap<OrderId, Order*>::InsertResult::DuplicateKey:
        SIXCHANGE_LOG_ERROR(
            "Duplicate engine order ID sequence={}, order_id={}",
            command.seq,
            order_id
        );

        orders_.release(order);

        return std::unexpected{RejectReason::MatchingEngineError};

    case FixedOrderMap<OrderId, Order*>::InsertResult::Full:
        SIXCHANGE_LOG_WARNING(
            "Order lookup map exhausted sequence={}, order_id={}, capacity={}",
            command.seq,
            order_id,
            orders_by_id_.capacity()
        );

        orders_.release(order);

        return std::unexpected{RejectReason::CapacityExhausted};
    }

    return order;
}

NewOrderOutcome OrderBook::execute(Order* aggressor, const MatchSink match_sink) noexcept {
    assert(aggressor != nullptr);
    assert(aggressor->active);

    match(aggressor, match_sink);

    if (const Quantity remaining = aggressor->remaining_quantity; remaining > Quantity{0}) {
        rest(aggressor);

        return NewOrderOutcome{
            .remaining_quantity = remaining,
            .rested = true
        };
    }

    if (const bool erased = orders_by_id_.erase(aggressor->order_id); !erased) {
        SIXCHANGE_LOG_CRITICAL(
            "Aggressing order lookup erase failed after full fill order_id={}",
            aggressor->order_id
        );

        assert(false);
    }

    orders_.release(aggressor);

    return NewOrderOutcome{
        .remaining_quantity = Quantity{0},
        .rested = false
    };
}

OrderBook::CancelResult OrderBook::cancel(const CancelOrderCommand& command) noexcept {
    if (command.symbol_id != symbol_id_) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected sequence={}, order_id={}, reason={}",
            command.seq,
            command.order_id,
            names::reject_reason(RejectReason::UnknownSymbol)
        );

        return std::unexpected{RejectReason::UnknownSymbol};
    }

    const std::optional<Order*> found = orders_by_id_.find(command.order_id);

    if (!found || *found == nullptr) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected sequence={}, order_id={}, reason={}",
            command.seq,
            command.order_id,
            names::reject_reason(RejectReason::UnknownOrder)
        );

        return std::unexpected{RejectReason::UnknownOrder};
    }

    const auto order = *found;

    if (order->client_id != command.client_id || order->client_order_id != command.client_order_id) {
        SIXCHANGE_LOG_DEBUG(
            "Cancel rejected sequence={}, order_id={}, client_order_id={}, reason={}",
            command.seq,
            command.order_id,
            command.client_order_id,
            names::reject_reason(RejectReason::NotOrderOwner)
        );

        return std::unexpected{RejectReason::NotOrderOwner};
    }

    const std::size_t index = order->price;

    if (order->side == Side::Buy) {
        PriceLevel& level = bids_[index];

        level.remove(order);
        --active_bid_orders_;

        if (best_bid_ && *best_bid_ == index && level.empty()) {
            update_best_bid_after_removal(index);
        }
    } else {
        PriceLevel& level = asks_[index];

        level.remove(order);
        --active_ask_orders_;

        if (best_ask_ && *best_ask_ == index && level.empty()) {
            update_best_ask_after_removal(index);
        }
    }

    if (const bool erased = orders_by_id_.erase(command.order_id); !erased) {
        SIXCHANGE_LOG_CRITICAL(
            "Order lookup erase failed after price-level removal sequence={}, order_id={}",
            command.seq,
            command.order_id
        );

        return std::unexpected{RejectReason::MatchingEngineError};
    }

    const CancelledOrder cancelled{
        .order_id = order->order_id,
        .client_order_id = order->client_order_id,
        .client_id = order->client_id,
        .symbol_id = order->symbol_id,
        .side = order->side,
        .price = order->price,
        .cancelled_quantity = order->remaining_quantity
    };

    SIXCHANGE_LOG_DEBUG(
        "Order cancelled sequence={}, order_id={}, client_order_id={}, side={}, price={}, cancelled_quantity={}",
        command.seq,
        order->order_id,
        order->client_order_id,
        names::side(order->side),
        order->price,
        order->remaining_quantity
    );

    orders_.release(order);

    return cancelled;
}

void OrderBook::rest(Order* order) noexcept {
    assert(order != nullptr);
    assert(order->remaining_quantity > 0);
    assert(order->symbol_id == symbol_id_);

    const std::size_t index = order->price;

    if (order->side == Side::Buy) {
        PriceLevel& level = bids_[index];
        level.push_back(order);
        ++active_bid_orders_;
        if (!best_bid_ || index > *best_bid_) {
            best_bid_ = index;
        }
    }
    else {
        PriceLevel& level = asks_[index];
        level.push_back(order);
        ++active_ask_orders_;
        if (!best_ask_ || index < *best_ask_) {
            best_ask_ = index;
        }
    }

    SIXCHANGE_LOG_DEBUG(
        "Order rested sequence={}, order_id={}, client_order_id={}, side={}, price={}, remaining_quantity={}",
        order->seq,
        order->order_id,
        order->client_order_id,
        names::side(order->side),
        order->price,
        order->remaining_quantity
    );
}

void OrderBook::update_best_bid_after_removal(const std::size_t removed_index) noexcept {
    if (active_bid_orders_ == 0) {
        best_bid_.reset();
        return;
    }

    for (auto index{removed_index}; index > 0; --index) {
        if (auto candidate{index - 1}; !bids_[candidate].empty()) {
            best_bid_ = candidate;
            return;
        }
    }
    best_bid_.reset();
}

void OrderBook::update_best_ask_after_removal(const std::size_t removed_index) noexcept {
    if (active_ask_orders_ == 0) {
        best_ask_.reset();
        return;
    }

    for (auto candidate{removed_index + 1}; candidate < config_.price_level_count; ++candidate) {
        if (!asks_[candidate].empty()) {
            best_ask_ = candidate;
            return;
        }
    }

    best_ask_.reset();
}

void OrderBook::match(Order* aggressor, const MatchSink match_sink) noexcept {
    assert(aggressor != nullptr);

    if (aggressor->side == Side::Buy) {
        match_buy(aggressor, match_sink);
    } else {
        match_sell(aggressor, match_sink);
    }
}

void OrderBook::match_buy(Order* aggressor, const MatchSink match_sink) noexcept {
    while (aggressor->remaining_quantity > 0 && best_ask_ && aggressor->price >= *best_ask_) {
        const std::size_t level_index = *best_ask_;
        PriceLevel& level = asks_[level_index];
        Order* resting = level.front();

        assert(resting != nullptr);
        assert(resting->side == Side::Sell);

        const Quantity executed_quantity = std::min(aggressor->remaining_quantity, resting->remaining_quantity);
        ++execution_count_;
        const Price execution_price = resting->price;

        level.total_quantity -= executed_quantity;
        aggressor->remaining_quantity -= executed_quantity;
        resting->remaining_quantity -= executed_quantity;

        SIXCHANGE_LOG_DEBUG(
            "Trade executed aggressing_order_id={}, resting_order_id={}, price={}, quantity={}",
            aggressor->order_id,
            resting->order_id,
            execution_price,
            executed_quantity
        );

        match_sink.emit(
           MatchExecution{
               .seq = aggressor->seq,
               .symbol_id = aggressor->symbol_id,
               .aggressing_side = aggressor->side,
               .price = execution_price,
               .quantity = executed_quantity,
               .aggressing_order_id = aggressor->order_id,
               .aggressing_client_order_id = aggressor->client_order_id,
               .aggressing_client_id = aggressor->client_id,
               .aggressing_remaining_quantity = aggressor->remaining_quantity,
               .resting_order_id = resting->order_id,
               .resting_client_order_id = resting->client_order_id,
               .resting_client_id = resting->client_id,
               .resting_remaining_quantity = resting->remaining_quantity
           }
       );

        if (resting->remaining_quantity == 0) {
            level.remove(resting);
            const bool erased [[maybe_unused]] = orders_by_id_.erase(resting->order_id);
            assert(erased);

            --active_ask_orders_;
            orders_.release(resting);

            if (level.empty()) {
                update_best_ask_after_removal(level_index);
            }
        }
    }
}

void OrderBook::match_sell(Order* aggressor, const MatchSink match_sink) noexcept {
    while (aggressor->remaining_quantity > 0 && best_bid_ && aggressor->price <= *best_bid_) {
        const std::size_t level_index = *best_bid_;
        PriceLevel& level = bids_[level_index];
        Order* resting = level.front();

        assert(resting != nullptr);
        assert(resting->side == Side::Buy);

        const Quantity executed_quantity = std::min(aggressor->remaining_quantity, resting->remaining_quantity);
        ++execution_count_;
        const Price execution_price = resting->price;

        level.total_quantity -= executed_quantity;
        aggressor->remaining_quantity -= executed_quantity;
        resting->remaining_quantity -= executed_quantity;

        SIXCHANGE_LOG_DEBUG(
            "Trade executed aggressing_order_id={}, resting_order_id={}, price={}, quantity={}",
            aggressor->order_id,
            resting->order_id,
            execution_price,
            executed_quantity
        );

        match_sink.emit(
           MatchExecution{
               .seq = aggressor->seq,
               .symbol_id = aggressor->symbol_id,
               .aggressing_side = aggressor->side,
               .price = execution_price,
               .quantity = executed_quantity,
               .aggressing_order_id = aggressor->order_id,
               .aggressing_client_order_id = aggressor->client_order_id,
               .aggressing_client_id = aggressor->client_id,
               .aggressing_remaining_quantity = aggressor->remaining_quantity,
               .resting_order_id = resting->order_id,
               .resting_client_order_id = resting->client_order_id,
               .resting_client_id = resting->client_id,
               .resting_remaining_quantity = resting->remaining_quantity
           }
       );

        if (resting->remaining_quantity == 0) {
            level.remove(resting);

            const bool erased [[maybe_unused]] = orders_by_id_.erase(resting->order_id);
            assert(erased);

            --active_bid_orders_;
            orders_.release(resting);

            if (level.empty()) {
                update_best_bid_after_removal(level_index);
            }
        }
    }
}

} // namespace sixchange
