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
    explicit OrderGateway(MatchingEngine& engine) noexcept;

    [[nodiscard]] protocol::OutboundMessage handle(const protocol::InboundMessage& message);

private:
    [[nodiscard]] protocol::OutboundMessage handle_new_order(const protocol::NewOrderRequest& request);

    [[nodiscard]] static std::optional<SymbolId> resolve_symbol(std::string_view symbol) noexcept;

    MatchingEngine& engine_;
    ClientId client_id_{1};

    std::unordered_map<ClientOrderId, OrderId> client_orders_;
};

} // namespace sixchange
