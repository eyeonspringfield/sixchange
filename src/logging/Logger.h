#pragma once

#include <cstddef>
#include <source_location>
#include <string_view>
#include <utility>

#include <sixchange/logging/LogLevel.h>
#include "LogRecord.h"

namespace sixchange::logging {
class Logger {
public:
    virtual ~Logger() = default;

    template <std::size_t FormatLength, typename... Args>
    [[nodiscard]] bool enqueue(const LogLevel level,
                               const std::source_location location,
                               const char (&format)[FormatLength], Args&&... args) noexcept {
        if (!should_log(level)) {
            return true;
        }

        static_assert(FormatLength > 0, "Log format must not be empty storage");

        const LogRecord record{
            .level = level,
            .location = location,
            .format = std::string_view{format, FormatLength - 1},
            .arguments = capture_arguments(std::forward<Args>(args)...)
        };

        return enqueue_record(record);
    }

private:
    [[nodiscard]]
    virtual bool should_log(LogLevel level) const noexcept = 0;

    [[nodiscard]]
    virtual bool enqueue_record(const LogRecord& record) noexcept = 0;
};
} // namespace sixchange::logging
