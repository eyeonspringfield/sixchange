#include <iostream>
#include <string>

#include "engine/MatchingEngine.h"
#include "engine/Sequencer.h"
#include "gateway/OrderGateway.h"
#include "protocol/TextCodec.h"
#include "logging/Log.h"
#include "logging/AsyncFileLogger.h"

int main() {
    using namespace sixchange;

    logging::AsyncFileLogger logger{"sixchange.log", logging::LogLevel::Debug};
    logging::set_thread_logger(logger);

    MatchingEngine engine{SymbolId{0}};
    OrderGateway gateway{engine};

    std::string line;

    while (std::getline(std::cin, line)) {
        protocol::TextCodec codec;
        const auto decoded = codec.decode(line);

        if (!decoded) {
            const protocol::OutboundMessage response{
                decoded.error()
            };

            std::cout << codec.encode(response) << '\n';

            std::cout.flush();
            continue;
        }

        const protocol::OutboundMessage response =
            gateway.handle(*decoded);

        std::cout << codec.encode(response) << '\n';

        std::cout.flush();
    }

    if (!std::cin.eof()) {
        std::cerr << "Failed while reading standard input.\n";

        return 1;
    }

    logging::clear_thread_logger();

    return 0;
}
