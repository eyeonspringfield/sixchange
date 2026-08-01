#include "Log.h"
#include "NullLogger.h"

namespace {

thread_local sixchange::logging::Logger* current_logger_pointer = nullptr;

[[nodiscard]] sixchange::logging::NullLogger& fallback_logger() noexcept {
    static sixchange::logging::NullLogger logger;

    return logger;
}

} // anonymous namespace

namespace sixchange::logging {

void set_thread_logger(Logger& logger) noexcept {
    current_logger_pointer = &logger;
}

void clear_thread_logger() noexcept {
    current_logger_pointer = nullptr;
}

Logger& current_logger() noexcept {
    if (current_logger_pointer == nullptr) {
        return fallback_logger();
    }

    return *current_logger_pointer;
}

} // namespace sixchange::logging
