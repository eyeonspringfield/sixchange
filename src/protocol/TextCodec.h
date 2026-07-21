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

    [[nodiscard]] static DecodeResult decode(std::string_view message) noexcept;

    [[nodiscard]] static std::string encode(const OutboundMessage& message);
};

} // namespace sixchange::protocol
