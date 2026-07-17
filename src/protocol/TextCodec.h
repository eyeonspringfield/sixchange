#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <sixchange/protocol/Messages.h>

namespace sixchange::protocol {

    class TextCodec {
    public:
        static constexpr std::size_t max_message_length = 256;

        using DecodeResult = std::expected<InboundMessage, OrderRejected>;

        [[nodiscard]] DecodeResult decode(std::string_view message) const noexcept;

        [[nodiscard]] std::string encode(const OutboundMessage &message) const;
    };

} // namespace sixchange::protocol
