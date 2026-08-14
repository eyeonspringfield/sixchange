#pragma once

#include <cassert>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>

#include <sixchange/core/Commands.h>
#include <sixchange/config/OrderBookConfig.h>

#include "FixedOrderMap.h"
#include "OrderPool.h"
#include "PriceLevel.h"

namespace sixchange {

class OrderBook {
public:
    explicit OrderBook(SymbolId symbol_id, const OrderBookConfig& config = DefaultOrderBookConfig);

    using AddResult = std::expected<void, RejectReason>;
    using CancelResult = std::expected<void, RejectReason>;

    AddResult add(const NewOrderCommand& command, const OrderId& order_id) noexcept;

    CancelResult cancel(const CancelOrderCommand& command) noexcept;

    [[nodiscard]] const PriceLevel& bid_level(const Price price) const noexcept {
        assert(price < config_.price_level_count);
        return bids_[price_index(price)];
    }

    [[nodiscard]] const PriceLevel& ask_level(const Price price) const noexcept {
        assert(price < config_.price_level_count);
        return asks_[price_index(price)];
    }

    [[nodiscard]] std::optional<Price> best_bid() const noexcept {
        if (!best_bid_) {
            return std::nullopt;
        }
        return *best_bid_;
    }

    [[nodiscard]] std::optional<Price> best_ask() const noexcept {
        if (!best_ask_) {
            return std::nullopt;
        }
        return *best_ask_;
    }

    [[nodiscard]] std::uint64_t execution_count() const noexcept {
        return execution_count_;
    }

private:
    [[nodiscard]] static constexpr std::size_t price_index(const Price price) noexcept {
        return static_cast<std::size_t>(price);
    }

    void match(Order* aggressor) noexcept;

    void match_buy(Order* aggressor) noexcept;
    void match_sell(Order* aggressor) noexcept;

    void rest(Order* order) noexcept;

    void update_best_bid_after_removal(std::size_t removed_index) noexcept;

    void update_best_ask_after_removal(std::size_t removed_index) noexcept;

    SymbolId symbol_id_{};

    OrderBookConfig config_{};

    OrderPool orders_;

    FixedOrderMap<OrderId, Order*> orders_by_id_;

    std::unique_ptr<PriceLevel[]> bids_{};
    std::unique_ptr<PriceLevel[]> asks_{};

    std::optional<std::size_t> best_bid_{};
    std::optional<std::size_t> best_ask_{};

    std::size_t active_bid_orders_{0};
    std::size_t active_ask_orders_{0};

    std::uint64_t execution_count_{0};

    //TODO: MATCHING

};

} // namespace sixchange
