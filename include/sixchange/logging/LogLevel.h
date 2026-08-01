#pragma once

#include <cstdint>

namespace sixchange::logging {

enum class LogLevel : std::uint8_t {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

} // namespace sixchange::logging