#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace sixchange {

template <std::unsigned_integral Key, typename Value>
class FixedOrderMap final {
    static_assert(
        std::is_nothrow_default_constructible_v<Value>,
        "FixedOrderMap values must be nothrow default constructible"
    );

    static_assert(
        std::is_nothrow_copy_assignable_v<Value>,
        "FixedOrderMap values must be nothrow copy assignable"
    );

public:
    enum class InsertResult : std::uint8_t {
        Inserted,
        DuplicateKey,
        Full
    };

    explicit FixedOrderMap(const std::size_t capacity)
        : capacity_{validate_capacity(capacity)},
          mask_{capacity_ - 1},
          entries_{std::make_unique<Entry[]>(capacity_)} {
    }

    [[nodiscard]] InsertResult insert(const Key key, const Value value) noexcept {
        std::size_t index = bucket(key);

        for (std::size_t probe{0}; probe < capacity_; ++probe) {
            Entry& entry = entries_[index];

            if (!entry.occupied) {
                entry.key = key;
                entry.value = value;
                entry.occupied = true;

                ++size_;
                return InsertResult::Inserted;
            }

            if (entry.key == key) {
                return InsertResult::DuplicateKey;
            }

            index = next_bucket(index);
        }

        return InsertResult::Full;
    }

    [[nodiscard]] std::optional<Value> find(const Key key) const noexcept {
        const std::size_t index = find_index(key);

        if (index == capacity_) {
            return std::nullopt;
        }

        return entries_[index].value;
    }

    [[nodiscard]]
    bool contains(const Key key) const noexcept {
        return find_index(key) != capacity_;
    }

    [[nodiscard]]
    bool erase(const Key key) noexcept {
        const std::size_t index = find_index(key);

        if (index == capacity_) {
            return false;
        }

        std::size_t hole = index;
        entries_[hole] = Entry{};
        --size_;

        std::size_t scan = next_bucket(hole);

        for (std::size_t examined{0}; examined < capacity_ - 1; ++examined) {
            if (!entries_[scan].occupied) {
                break;
            }

            const std::size_t home = bucket(entries_[scan].key);

            if (probe_distance(home, hole) < probe_distance(home, scan)) {
                entries_[hole] = entries_[scan];
                entries_[scan] = Entry{};
                hole = scan;
            }

            scan = next_bucket(scan);
        }

        return true;
    }

    [[nodiscard]]
    std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]]
    std::size_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]]
    bool empty() const noexcept {
        return size_ == 0;
    }

private:
    struct Entry {
        Key key{};
        Value value{};
        bool occupied{};
    };

    [[nodiscard]] static std::size_t validate_capacity(const std::size_t capacity) {
        if (!std::has_single_bit(capacity)) {
            throw std::invalid_argument{
                "FixedOrderMap capacity must be a non-zero power of two"
            };
        }

        return capacity;
    }

    [[nodiscard]] std::size_t bucket(const Key key) const noexcept {
        std::uint64_t value =
            static_cast<std::uint64_t>(key);

        value += 0x9e3779b97f4a7c15ULL;
        value =
            (value ^ (value >> 30U)) *
            0xbf58476d1ce4e5b9ULL;
        value =
            (value ^ (value >> 27U)) *
            0x94d049bb133111ebULL;
        value ^= value >> 31U;

        return value & mask_;
    }

    [[nodiscard]] std::size_t next_bucket(const std::size_t index) const noexcept {
        return (index + 1) & mask_;
    }

    [[nodiscard]] std::size_t probe_distance(const std::size_t from, const std::size_t to) const noexcept {
        return (to - from) & mask_;
    }

    [[nodiscard]] std::size_t find_index(const Key key) const noexcept {
        std::size_t index = bucket(key);

        for (std::size_t probe{0}; probe < capacity_; ++probe) {
            const Entry& entry = entries_[index];

            if (!entry.occupied) {
                return capacity_;
            }

            if (entry.key == key) {
                return index;
            }

            index = next_bucket(index);
        }

        return capacity_;
    }

    std::size_t capacity_{};
    std::size_t mask_{};
    std::unique_ptr<Entry[]> entries_;
    std::size_t size_{};
};

} // namespace sixchange
