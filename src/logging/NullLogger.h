#pragma once

#include "Logger.h"

namespace sixchange::logging {

class NullLogger final : public Logger {
    [[nodiscard]] bool should_log(LogLevel) const noexcept override {
        return false;
    }

    [[nodiscard]] bool enqueue_record(const LogRecord&) noexcept override {
        return true;
    }
};

} // namespace sixchange::logging