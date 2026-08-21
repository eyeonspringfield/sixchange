#pragma once

#include <sixchange/config/OrderBookConfig.h>

namespace sixchange::test {

inline constexpr OrderBookConfig OrderBookConfig {
    .max_active_orders = 64,
    .price_level_count = 1'024,
    .order_lookup_capacity = 128
};

} // namespace sixchange::test