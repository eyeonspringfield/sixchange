#include "logging/AsyncFileLogger.h"

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

[[nodiscard]] std::optional<std::size_t> parse_argument_index(const std::string_view value) noexcept {
    if (value.empty()) {
        return std::nullopt;
    }

    std::size_t result = 0;

    for (const char character : value) {
        if (const auto unsigned_character = static_cast<unsigned char>(character); std::isdigit(unsigned_character) == 0) {
            return std::nullopt;
        }

        result = result * 10 + static_cast<std::size_t>(character - '0');
    }

    return result;
}

} // namespace

namespace sixchange::logging {

AsyncFileLogger::AsyncFileLogger(const std::string& path,const LogLevel minimum_level)
    : minimum_level_{minimum_level},
    output_{path, std::ios::out | std::ios::app} {
    if (!output_) {
        throw std::runtime_error{
            "Failed to open log file: " + path
        };
    }

    worker_ = std::jthread{[this](const std::stop_token stop_token) {
            run(stop_token);
        }
    };
}

AsyncFileLogger::~AsyncFileLogger() {
    worker_.request_stop();

    if (worker_.joinable()) {
        worker_.join();
    }

    output_.flush();
}

std::uint64_t  AsyncFileLogger::dropped_count() const noexcept {
    return dropped_records_.load(std::memory_order_relaxed);
}

bool AsyncFileLogger::should_log(const LogLevel level) const noexcept {
    return std::to_underlying(level) >= std::to_underlying(minimum_level_);
}

bool AsyncFileLogger::enqueue_record(const LogRecord& record) noexcept {
    if (queue_.try_push(record)) {
        return true;
    }

    dropped_records_.fetch_add(1,std::memory_order_relaxed);

    return false;
}

void AsyncFileLogger::run(const std::stop_token stop_token) {
    using namespace std::chrono_literals;

    LogRecord record{};

    auto last_flush = std::chrono::steady_clock::now();

    while (!stop_token.stop_requested() || !queue_.empty()) {
        std::size_t processed = 0;

        while (processed < 1'024 && queue_.try_pop(record)) {
            write_record(record);
            ++processed;
        }

        const std::uint64_t dropped = dropped_records_.exchange(0,std::memory_order_relaxed);

        if (dropped != 0) {
            write_dropped_records(dropped);
        }

        const auto now = std::chrono::steady_clock::now();

        if (now - last_flush >= 1s) {
            output_.flush();
            last_flush = now;
        }

        if (processed == 0 && dropped == 0) {
            std::this_thread::sleep_for(200us);
        }
    }

    const std::uint64_t remaining_dropped = dropped_records_.exchange(0, std::memory_order_relaxed);

    if (remaining_dropped != 0) {
        write_dropped_records(remaining_dropped);
    }

    output_.flush();
}

void AsyncFileLogger::write_record(const LogRecord& record) {
    write_timestamp();

    output_
        << ' '
        << level_name(record.level)
        << ' '
        << short_function_name(record.location.function_name())
        << ' ';

    write_formatted_message(record.format,record.arguments);

    output_ << '\n';
}

void AsyncFileLogger::write_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now - seconds);
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
    localtime_r(&time, &local_time);

    output_
        << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S")
        << '.'
        << std::setfill('0')
        << std::setw(6)
        << microseconds.count()
        << std::setfill(' ');
}

void AsyncFileLogger::write_formatted_message(const std::string_view format, const CapturedArguments& arguments) {
    std::size_t position = 0;
    std::size_t automatic_index = 0;

    while (position < format.size()) {
        if (format[position] == '{') {
            if (position + 1 < format.size() && format[position + 1] == '{') {
                output_ << '{';
                position += 2;
                continue;
            }

            const std::size_t closing = format.find('}', position + 1);

            if (closing == std::string_view::npos) {
                output_.write(format.data() + position, static_cast<std::streamsize>(format.size() - position));
                return;
            }

            const std::string_view placeholder = format.substr(position + 1, closing - position - 1);

            std::optional<std::size_t> argument_index;

            if (placeholder.empty()) {
                argument_index = automatic_index++;
            }
            else {
                argument_index = parse_argument_index(placeholder);
            }

            if (argument_index && *argument_index < arguments.count) {
                write_argument(arguments.values[*argument_index]);
            }
            else {
                output_.write(format.data() + position,static_cast<std::streamsize>(closing - position + 1));
            }

            position = closing + 1;
            continue;
        }

        if (format[position] == '}' && position + 1 < format.size() && format[position + 1] == '}') {
            output_ << '}';
            position += 2;
            continue;
        }

        output_ << format[position];
        ++position;
    }
}

void AsyncFileLogger::write_argument(const LogArgument& argument) {
    std::visit([this]<typename T>(const T& value) {
            using Value = std::remove_cvref_t<T>;

            if constexpr (std::same_as<Value, bool>) {
                output_ << (value ? "true" : "false");
            } else if constexpr (std::same_as<Value, InlineLogString>) {
                output_ << value.view();

                if (value.truncated) {
                    output_ << "...";
                }
            }
            else {
                output_ << value;
            }
        },
        argument
    );
}

void AsyncFileLogger::write_dropped_records(
    const std::uint64_t count) {
    write_timestamp();

    output_
        << " WARNING"
        << " AsyncFileLogger::run"
        << " Dropped log records count="
        << count
        << '\n';
}

std::string_view AsyncFileLogger::level_name(const LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";

    case LogLevel::Debug:
        return "DEBUG";

    case LogLevel::Info:
        return "INFO";

    case LogLevel::Warning:
        return "WARNING";

    case LogLevel::Error:
        return "ERROR";

    case LogLevel::Critical:
        return "CRITICAL";
    }

    return "UNKNOWN";
}

std::string_view AsyncFileLogger::short_function_name(std::string_view full_name) noexcept {
    const std::size_t parameters = full_name.find('(');

    if (parameters != std::string_view::npos) {
        full_name = full_name.substr(0, parameters);
    }

    const std::size_t final_space = full_name.rfind(' ');

    if (final_space != std::string_view::npos) {
        full_name = full_name.substr(final_space + 1);
    }

    const std::size_t final_separator = full_name.rfind("::");

    if (final_separator == std::string_view::npos) {
        return full_name;
    }

    if (final_separator == 0) {
        return full_name;
    }

    const std::size_t previous_separator = full_name.rfind("::",final_separator - 1);

    if (previous_separator == std::string_view::npos) {
        return full_name;
    }

    return full_name.substr(previous_separator + 2);
}
} // namespace sixchange::logging
