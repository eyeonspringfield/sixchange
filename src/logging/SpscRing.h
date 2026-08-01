#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace sixchange::logging {

template <typename T, std::size_t Capacity>
class SpscRing final {
    static_assert(std::has_single_bit(Capacity), "SPSC capacity must be a power of two");

    static_assert(std::is_nothrow_copy_assignable_v<T>, "SPSC elements must be nothrow copy assignable");

public:
    SpscRing() : storage_{std::make_unique<T[]>(Capacity)} {
    }

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);

        if (head - tail >= Capacity) {
            return false;
        }

        storage_[head & mask_] = value;

        head_.store(head + 1,std::memory_order_release);

        return true;
    }

    [[nodiscard]] bool try_pop(T& value) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);

        if (tail == head) {
            return false;
        }

        value = storage_[tail & mask_];

        tail_.store(tail + 1,std::memory_order_release);

        return true;
    }

    [[nodiscard]]
    bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

    [[nodiscard]]
    static constexpr std::size_t capacity()
        noexcept {
        return Capacity;
    }

private:
    static constexpr std::size_t mask_ =Capacity - 1;

    std::unique_ptr<T[]> storage_;

    alignas(64) std::atomic<std::size_t> head_{};

    alignas(64) std::atomic<std::size_t> tail_{};
};

} // namespace sixchange::logging
