#include "OrderBook.h"

#include <cassert>
#include <cstddef>
#include <expected>
#include <stdexcept>

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

OrderBook::AddResult OrderBook::add(const NewOrderCommand& command, const OrderId& order_id) noexcept {
    if (command.symbol_id != symbol_id_) {
        return std::unexpected{RejectReason::UnknownSymbol};
    }

    if (command.quantity == Quantity{0}) {
        return std::unexpected{RejectReason::InvalidQuantity};
    }

    if (command.price >= config_.price_level_count) {
        return std::unexpected{RejectReason::InvalidPrice};
    }

    Order* order = orders_.allocate();
    if (order == nullptr) {
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
        orders_.release(order);

        return std::unexpected{RejectReason::MatchingEngineError};

    case FixedOrderMap<OrderId, Order*>::InsertResult::Full:
        orders_.release(order);

        return std::unexpected{RejectReason::CapacityExhausted};
    }


    rest(order);

    return {};
}

OrderBook::CancelResult OrderBook::cancel(const CancelOrderCommand& command) noexcept {
    if (command.symbol_id != symbol_id_) {
        return std::unexpected{RejectReason::UnknownSymbol};
    }

    const std::optional<Order*> found = orders_by_id_.find(command.order_id);

    if (!found || *found == nullptr) {
        return std::unexpected{RejectReason::UnknownOrder};
    }

    const auto order = *found;

    if (order->client_id != command.client_id || order->client_order_id != command.client_order_id) {
        return std::unexpected{RejectReason::NotOrderOwner};
    }

    const std::size_t index = price_index(order->price);

    if (order->side == Side::Buy) {
        PriceLevel& level = bids_[index];

        level.remove(order);

        if (best_bid_ && *best_bid_ == index && level.empty()) {
            update_best_bid_after_removal(index);
        }
    } else {
        PriceLevel& level = asks_[index];

        level.remove(order);

        if (best_ask_ && *best_ask_ == index && level.empty()) {
            update_best_ask_after_removal(index);
        }
    }

    const bool erased = orders_by_id_.erase(command.order_id);

    if (!erased) {
        return std::unexpected{RejectReason::None};
    }

    orders_.release(order);

    return {};
}

void OrderBook::rest(Order* order) noexcept {
    assert(order != nullptr);
    assert(order->remaining_quantity > 0);
    assert(order->symbol_id == symbol_id_);

    const std::size_t index = price_index(order->price);

    if (order->side == Side::Buy) {
        PriceLevel& level = bids_[index];
        level.push_back(order);
        if (!best_bid_ || index > *best_bid_) {
            best_bid_ = index;
        }
    }
    else {
        PriceLevel& level = asks_[index];
        level.push_back(order);
        if (!best_ask_ || index < *best_ask_) {
            best_ask_ = index;
        }
    }
}

void OrderBook::update_best_bid_after_removal(const std::size_t removed_index) noexcept {
    for (auto index{removed_index}; index > 0; --index) {
        if (auto candidate{index - 1}; !bids_[candidate].empty()) {
            best_bid_ = candidate;
            return;
        }
    }
    best_bid_.reset();
}

void OrderBook::update_best_ask_after_removal(const std::size_t removed_index) noexcept {
    for (auto candidate{removed_index + 1}; candidate < config_.price_level_count; ++candidate) {
        if (!asks_[candidate].empty()) {
            best_ask_ = candidate;
            return;
        }
    }

    best_ask_.reset();
}

} // namespace sixchange
