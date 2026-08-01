#pragma once

#include <source_location>

#include "Logger.h"

namespace sixchange::logging {

void set_thread_logger(Logger& logger) noexcept;

void clear_thread_logger() noexcept;

[[nodiscard]] Logger& current_logger() noexcept;

} // namespace sixchange::logging

#define SIXCHANGE_LOG_IMPL(                               \
    level_value,                                          \
    format_literal,                                       \
    ...                                                   \
)                                                         \
    do {                                                  \
        (void)::sixchange::logging::                      \
            current_logger().enqueue(                     \
                (level_value),                            \
                std::source_location::current(),          \
                (format_literal)                          \
                    __VA_OPT__(,)                         \
                    __VA_ARGS__                           \
            );                                            \
    } while (false)

#define SIXCHANGE_LOG_TRACE(format_literal, ...)          \
    SIXCHANGE_LOG_IMPL(                                   \
        ::sixchange::logging::LogLevel::Trace,            \
        format_literal                                    \
            __VA_OPT__(,)                                 \
            __VA_ARGS__                                   \
    )

#define SIXCHANGE_LOG_DEBUG(format_literal, ...)          \
    SIXCHANGE_LOG_IMPL(                                   \
        ::sixchange::logging::LogLevel::Debug,            \
        format_literal                                    \
            __VA_OPT__(,)                                 \
            __VA_ARGS__                                   \
    )

#define SIXCHANGE_LOG_INFO(format_literal, ...)           \
    SIXCHANGE_LOG_IMPL(                                   \
        ::sixchange::logging::LogLevel::Info,             \
        format_literal                                    \
            __VA_OPT__(,)                                 \
            __VA_ARGS__                                   \
    )

#define SIXCHANGE_LOG_WARNING(format_literal, ...)        \
    SIXCHANGE_LOG_IMPL(                                   \
        ::sixchange::logging::LogLevel::Warning,          \
        format_literal                                    \
            __VA_OPT__(,)                                 \
            __VA_ARGS__                                   \
    )

#define SIXCHANGE_LOG_ERROR(format_literal, ...)          \
    SIXCHANGE_LOG_IMPL(                                   \
        ::sixchange::logging::LogLevel::Error,            \
        format_literal                                    \
            __VA_OPT__(,)                                 \
            __VA_ARGS__                                   \
    )

#define SIXCHANGE_LOG_CRITICAL(format_literal, ...)       \
    SIXCHANGE_LOG_IMPL(                                   \
        ::sixchange::logging::LogLevel::Critical,         \
        format_literal                                    \
            __VA_OPT__(,)                                 \
            __VA_ARGS__                                   \
    )
