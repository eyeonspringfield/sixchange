#pragma once

#include <cstddef>
#include <memory>

#include "Order.h"

namespace sixchange {

template <std::size_t Capacity>
class OrderPool {
public:
    OrderPool()
        : orders_(std::make_unique<Order[]>(Capacity)) {
    }

    [[nodiscard]]
    Order* allocate() noexcept {
        if (free_head_ != nullptr) {
            Order* order = free_head_;
            free_head_ = free_head_->next;
            *order = Order{};
            return order;
        }

        if (size_ == Capacity) {
            return nullptr;
        }

        return &orders_[size_++];
    }

    void release(Order* order) noexcept {
        *order = Order{};
        order->next = free_head_;
        free_head_ = order;
    }

private:
    std::unique_ptr<Order[]> orders_;

    std::size_t size_{};
    Order* free_head_{};
};

} // namespace sixchange
