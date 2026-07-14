#pragma once

#include <sixchange/core/Types.h>
#include <sixchange/core/Enums.h>

namespace sixchange {

    struct EngineEvent {
        SequenceNumber seq{};

        OrderId order_id{};
        OrderId resting_order_id{};
        OrderId aggressing_order_id{};

        Price price{};
        Quantity quantity{};
        Quantity remaining_quantity{};

        TradeId trade_id{};

        ClientOrderId client_order_id{};
        ClientId client_id{};
        SymbolId symbol_id{};

        Side side{};
        RejectReason reject_reason{RejectReason::None};
        LiquidityFlag liquidity{LiquidityFlag::None};
        EventType type{};
    };

} // namespace sixchange