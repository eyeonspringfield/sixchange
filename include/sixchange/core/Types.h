#pragma once

#include <cstdint>

namespace sixchange {

    using SequenceNumber    = std::uint64_t;
    using OrderId           = std::uint64_t;
    using TradeId           = std::uint64_t;
    using ClientId          = std::uint32_t;
    using ClientOrderId     = std::uint64_t;
    using SymbolId          = std::uint32_t;

    using Price             = std::uint64_t;
    using Quantity          = std::uint64_t;

} // namespace sixchange
