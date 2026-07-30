#pragma once

#include <cstddef>

namespace sixchange {

struct OrderBookConfig {
    std::size_t max_active_orders;
    std::size_t price_level_count;
    std::size_t order_lookup_capacity;
};

inline constexpr OrderBookConfig DefaultOrderBookConfig{
    .max_active_orders = 262'144,
    .price_level_count = 100'000,
    .order_lookup_capacity = 524'288
};

} // namespace sixchange