#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

#include "Order.h"

namespace sixchange {

class OrderPool {
public:
    explicit OrderPool(const std::size_t capacity)
        : orders_{std::make_unique<Order[]>(capacity)},
          capacity_{capacity} {
        assert(capacity > 0);
    }

    [[nodiscard]] Order* allocate() noexcept {
        if (free_head_ != nullptr) {
            Order* const order = free_head_;
            free_head_ = free_head_->next;

            *order = Order{};
            return order;
        }

        if (size_ == capacity_) {
            return nullptr;
        }

        return &orders_[size_++];
    }

    void release(Order* const order) noexcept {
        assert(order != nullptr);

        *order = Order{};
        order->next = free_head_;
        free_head_ = order;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] std::size_t allocated_slots() const noexcept {
        return size_;
    }

private:
    std::unique_ptr<Order[]> orders_;

    std::size_t capacity_{};
    std::size_t size_{};

    Order* free_head_{};
};

} // namespace sixchange
