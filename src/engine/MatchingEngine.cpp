#include "MatchingEngine.h"

namespace sixchange {

MatchingEngine::MatchingEngine(const SymbolId symbol_id)
    : order_book_(std::make_unique<OrderBook>(symbol_id)) {
}

MatchingEngine::~MatchingEngine() = default;

MatchingEngine::MatchingEngine(MatchingEngine&&) noexcept = default;

MatchingEngine& MatchingEngine::operator=(MatchingEngine&&) noexcept = default;

MatchingEngine::ProcessResult MatchingEngine::process(const EngineCommand& command) noexcept {
    switch (command.type) {
    case CommandType::NewOrder:
        {
            const OrderId order_id = next_order_id_;

            const auto result = order_book_->add(command.new_order, order_id);

            if (!result) {
                return std::unexpected{result.error()};
            }

            ++next_order_id_;
            return order_id;
        }

    default:
        return std::unexpected{RejectReason::MatchingEngineError};
    }
}

} // namespace sixchange
