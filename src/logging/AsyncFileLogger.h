#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "Logger.h"

#include "SpscRing.h"

namespace sixchange::logging {

class AsyncFileLogger final : public Logger {
public:
    static constexpr std::size_t QueueCapacity = 16'384;

    explicit AsyncFileLogger(const std::string& path, LogLevel minimum_level = LogLevel::Info);

    ~AsyncFileLogger() override;

    AsyncFileLogger(const AsyncFileLogger&) = delete;

    AsyncFileLogger& operator=(const AsyncFileLogger&) = delete;

    [[nodiscard]] std::uint64_t dropped_count() const noexcept;

private:
    [[nodiscard]] bool should_log(LogLevel level) const noexcept override;

    [[nodiscard]] bool enqueue_record(const LogRecord& record) noexcept override;

    void run(std::stop_token stop_token);

    void write_record(const LogRecord& record);

    void write_timestamp();

    void write_formatted_message(std::string_view format, const CapturedArguments& arguments);

    void write_argument(const LogArgument& argument);

    void write_dropped_records(std::uint64_t count);

    [[nodiscard]] static std::string_view level_name(LogLevel level) noexcept;

    [[nodiscard]] static std::string_view short_function_name(std::string_view full_name) noexcept;

    SpscRing<LogRecord,QueueCapacity> queue_;

    std::atomic<std::uint64_t> dropped_records_{};

    LogLevel minimum_level_;

    std::ofstream output_;
    std::jthread worker_;
};

} // namespace sixchange::logging