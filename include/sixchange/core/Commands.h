#pragma once

#include <sixchange/core/Types.h>
#include <sixchange/core/Enums.h>

namespace sixchange {

struct NewOrderCommand {
    SequenceNumber seq;

    Price price;
    Quantity quantity;

    ClientOrderId client_order_id;
    ClientId client_id;
    SymbolId symbol_id;

    Side side;
    OrderType order_type;
    TimeInForce tif;
};

struct CancelOrderCommand {
    SequenceNumber seq;

    OrderId order_id;
    ClientOrderId client_order_id;
    ClientId client_id;
    SymbolId symbol_id;
};

struct ReplaceOrderCommand {
    SequenceNumber seq;

    Price new_price;
    Quantity new_quantity;

    ClientOrderId old_client_order_id;
    ClientOrderId new_client_order_id;

    ClientId client_id;
    SymbolId symbol_id;
};

struct EngineCommand {
    CommandType type;

    union {
        NewOrderCommand new_order;
        CancelOrderCommand cancel_order;
        ReplaceOrderCommand replace_order;
    };
};

} // namespace sixchange
