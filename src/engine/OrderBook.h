#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <sixchange/core/Commands.h>

#include "OrderPool.h"
#include "PriceLevel.h"

namespace sixchange {
    inline constexpr std::size_t OrderBookMaxOrders = 100'000;
    inline constexpr std::size_t OrderBookMaxPriceTicks = 100'000;
    inline constexpr std::size_t OrderBookMaxEventsPerCommand = 128;

    class OrderBook {
    public:
        explicit OrderBook(SymbolId symbol_id);

        void add(const NewOrderCommand &command, const OrderId &order_id) noexcept;

        [[nodiscard]] const PriceLevel& bid_level(const Price price) const noexcept {
            return bids_[price_index(price)];
        }

        [[nodiscard]] const PriceLevel& ask_level(const Price price) const noexcept {
            return asks_[price_index(price)];
        }

    private:
        [[nodiscard]] static constexpr std::size_t price_index(const Price price) noexcept {
            return static_cast<std::size_t>(price);
        }

        SymbolId symbol_id_{};

        OrderPool<OrderBookMaxOrders> orders_{};

        std::array<PriceLevel, OrderBookMaxPriceTicks> bids_{};
        std::array<PriceLevel, OrderBookMaxPriceTicks> asks_{};

        std::optional<std::size_t> best_bid_{};
        std::optional<std::size_t> best_ask_{};

        //TODO: MATCHING

        void rest(Order *order) noexcept;
    };
} // namespace sixchange
