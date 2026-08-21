#pragma once

#include <sixchange/core/Types.h>
#include <sixchange/core/Enums.h>

namespace sixchange {

struct MatchExecution {
    SequenceNumber seq{};

    SymbolId symbol_id{};
    Side aggressing_side{};

    Price price{};
    Quantity quantity{};

    OrderId aggressing_order_id{};
    ClientOrderId aggressing_client_order_id{};
    ClientId aggressing_client_id{};
    Quantity aggressing_remaining_quantity{};

    OrderId resting_order_id{};
    ClientOrderId resting_client_order_id{};
    ClientId resting_client_id{};
    Quantity resting_remaining_quantity{};
};

} // namespace sixchange