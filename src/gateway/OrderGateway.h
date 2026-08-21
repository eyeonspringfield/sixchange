#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include <sixchange/core/Events.h>
#include <sixchange/protocol/Messages.h>
#include <sixchange/protocol/OutboundMessageSink.h>

#include "engine/MatchingEngine.h"
#include "engine/Sequencer.h"

namespace sixchange {

class OrderGateway {
public:
    OrderGateway(MatchingEngine& engine, protocol::OutboundMessageSink outbound_sink) noexcept;

    void handle(const protocol::InboundMessage& message) noexcept;

    void on_engine_event(const EngineEvent& event) noexcept;

private:
    void handle_new_order(const protocol::NewOrderRequest& request) noexcept;

    void handle_cancel_order(const protocol::CancelOrderRequest& request) noexcept;

    void emit_rejection(std::optional<ClientOrderId> client_order_id, RejectReason reason) const noexcept;

    [[nodiscard]] static std::optional<SymbolId> resolve_symbol(std::string_view symbol) noexcept;

    MatchingEngine& engine_;
    protocol::OutboundMessageSink outbound_sink_;
    ClientId client_id_{1};

    std::unordered_map<ClientOrderId, OrderId> client_orders_;
};

} // namespace sixchange
