#include "OrderBook.h"

#include <cassert>
#include <cstddef>
#include <functional>

namespace sixchange {

OrderBook::OrderBook(const SymbolId symbol_id) : symbol_id_(symbol_id) {
    for (std::size_t i{0}; i < OrderBookMaxPriceTicks; ++i) {
        bids_[i].price = i;
        asks_[i].price = i;
    }
}

OrderBook::AddResult OrderBook::add(const NewOrderCommand& command, const OrderId& order_id) noexcept {
    if (command.symbol_id != symbol_id_) {
        return std::unexpected{RejectReason::UnknownSymbol};
    }

    if (command.quantity <= 0) {
        return std::unexpected{RejectReason::InvalidQuantity};
    }

    if (command.price >= OrderBookMaxPriceTicks) {
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

    rest(order);

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

} // namespace sixchange
