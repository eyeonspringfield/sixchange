#include <iostream>
#include <string>

#include <sixchange/core/EngineEventDispatcher.h>
#include <sixchange/protocol/OutboundMessageSink.h>

#include "engine/MatchingEngine.h"
#include "gateway/OrderGateway.h"
#include "logging/AsyncFileLogger.h"
#include "logging/Log.h"
#include "protocol/TextCodec.h"

namespace {

class CliOutbound {
public:
    explicit CliOutbound(sixchange::protocol::TextCodec& codec) noexcept
        : codec_{codec} {
    }

    void on_outbound_message(const sixchange::protocol::OutboundMessage& message) noexcept {
        std::cout << codec_.encode(message) << '\n';
        std::cout.flush();
    }

private:
    sixchange::protocol::TextCodec& codec_;
};

} // namespace

int main() {
    using namespace sixchange;

    logging::AsyncFileLogger logger{"sixchange.log", logging::LogLevel::Warning};
    logging::set_thread_logger(logger);

    protocol::TextCodec codec;
    CliOutbound outbound{codec};

    EngineEventDispatcher<> dispatcher;
    MatchingEngine engine{
        SymbolId{0},
        EngineEventSink::from(dispatcher)
    };
    OrderGateway gateway{
        engine,
        protocol::OutboundMessageSink::from(outbound)
    };

    if (!dispatcher.add_sink(EngineEventSink::from(gateway))) {
        std::cerr << "Failed to register order gateway event sink.\n";
        return 1;
    }

    std::string line;

    while (std::getline(std::cin, line)) {
        const auto decoded = codec.decode(line);

        if (!decoded) {
            outbound.on_outbound_message(
                protocol::OutboundMessage{decoded.error()}
            );
            continue;
        }

        gateway.handle(*decoded);
    }

    if (!std::cin.eof()) {
        std::cerr << "Failed while reading standard input.\n";
        return 1;
    }

    std::cerr << "execution= " << engine.execution_count() << '\n';
    return 0;
}
