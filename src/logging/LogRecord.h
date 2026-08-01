#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <sixchange/logging/LogLevel.h>

namespace sixchange::logging {

inline constexpr std::size_t MaximumLogArguments = 8;
inline constexpr std::size_t MaximumInlineStringLength = 48;

template <std::size_t Capacity>
struct InlineString {
    static_assert(Capacity <= std::numeric_limits<std::uint16_t>::max());

    std::array<char, Capacity> data{};
    std::uint16_t size{};
    bool truncated{};

    [[nodiscard]] std::string_view view() const noexcept {
        return {data.data(), static_cast<std::size_t>(size)};
    }
};

using InlineLogString = InlineString<MaximumInlineStringLength>;

using LogArgument = std::variant<std::int64_t, std::uint64_t, double, bool, InlineLogString>;

struct CapturedArguments {
    std::array<LogArgument, MaximumLogArguments> values{};
    std::uint8_t count{};
};

struct LogRecord {
    LogLevel level{};
    std::source_location location{};
    std::string_view format;
    CapturedArguments arguments;
};

template <std::size_t Capacity>
[[nodiscard]] InlineString<Capacity> make_inline_string(const std::string_view value) noexcept {
    InlineString<Capacity> result{};

    const std::size_t copied_size = std::min(value.size(), Capacity);

    if (copied_size != 0) {
        std::memcpy(result.data.data(), value.data(), copied_size);
    }

    result.size = static_cast<std::uint16_t>(copied_size);

    result.truncated = value.size() > Capacity;

    return result;
}

template <typename>
inline constexpr bool UnsupportedLogArgument = false;

template <typename T>
[[nodiscard]] LogArgument capture_argument(T&& value) noexcept {
    using Value = std::remove_cvref_t<T>;

    if constexpr (std::same_as<Value, bool>) {
        return value;
    } else if constexpr (std::is_enum_v<Value>) {
        using Underlying = std::underlying_type_t<Value>;

        return capture_argument(static_cast<Underlying>(value));
    } else if constexpr (std::signed_integral<Value>) {
        return static_cast<std::int64_t>(value);
    } else if constexpr (std::unsigned_integral<Value>) {
        return static_cast<std::uint64_t>(value);
    } else if constexpr (std::floating_point<Value>) {
        return static_cast<double>(value);
    } else if constexpr (std::same_as<Value, const char*> || std::same_as<Value, char*>) {
        const std::string_view text =
            value == nullptr
                ? std::string_view{"<null>"}
                : std::string_view{value};

        return make_inline_string<MaximumInlineStringLength>(text);
    } else if constexpr (std::constructible_from<std::string_view, T>) {
        return make_inline_string<MaximumInlineStringLength>(std::string_view{std::forward<T>(value)});
    } else {
        static_assert(UnsupportedLogArgument<Value>, "Unsupported asynchronous log argument");
    }

    return false;
}

template <typename... Args>
[[nodiscard]] CapturedArguments capture_arguments(Args&&... args) noexcept {
    static_assert(
        sizeof...(Args) <=
        MaximumLogArguments,
        "Too many asynchronous log arguments"
    );

    CapturedArguments result{};

    if constexpr (sizeof...(Args) != 0) {
        std::size_t index = 0;

        (
            (
                result.values[index++] = capture_argument(std::forward<Args>(args))
            ),
            ...
        );
    }

    result.count = static_cast<std::uint8_t>(sizeof...(Args));

    return result;
}

} // namespace sixchange::logging
