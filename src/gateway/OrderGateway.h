#pragma once

#include <unordered_map>
#include <string_view>
#include <optional>

#include <sixchange/protocol/Messages.h>

#include "engine/MatchingEngine.h"
#include "engine/Sequencer.h"

namespace sixchange {

    class OrderGateway {
    public:
        explicit OrderGateway(MatchingEngine &engine, Sequencer& sequencer) noexcept;

        [[nodiscard]] protocol::OutboundMessage handle(const protocol::InboundMessage &message);

    private:
        [[nodiscard]] protocol::OutboundMessage handle_new_order(const protocol::NewOrderRequest& request);

        [[nodiscard]] std::optional<SymbolId> resolve_symbol(std::string_view symbol) const noexcept;

        [[nodiscard]] OrderId next_order_id() noexcept;

        MatchingEngine &engine_;
        Sequencer& sequencer_;
        ClientId client_id_{1};
        OrderId next_order_id_{1};

        std::unordered_map<ClientOrderId, OrderId> client_orders_;
    };
} // namespace sixchange
