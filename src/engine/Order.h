#pragma once

#include <sixchange/core/Enums.h>
#include <sixchange/core/Types.h>

namespace sixchange {

struct Order {
    OrderId order_id{};
    ClientOrderId client_order_id{};
    ClientId client_id{};
    SymbolId symbol_id{};

    Side side{};
    Price price{};
    Quantity remaining_quantity{};
    SequenceNumber seq{};

    Order* prev{};
    Order* next{};

    bool active{false};
};

} // namespace sixchange
