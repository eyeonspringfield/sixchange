#include "MatchingEngine.h"

namespace sixchange {

    MatchingEngine::MatchingEngine(const SymbolId symbol_id)
        : order_book_(std::make_unique<OrderBook>(symbol_id)) {}

    MatchingEngine::~MatchingEngine() = default;

    MatchingEngine::MatchingEngine(MatchingEngine &&) noexcept = default;

    MatchingEngine &MatchingEngine::operator=(MatchingEngine &&) noexcept = default;

    void MatchingEngine::process(const EngineCommand &command) noexcept {
        switch (command.type) {
            case CommandType::NewOrder:
                order_book_->add(command.new_order, next_order_id_++);
                break;

            default:
                break;
        }
    }

} // namespace sixchange
