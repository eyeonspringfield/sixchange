#pragma once

#include <sixchange/core/Types.h>

#include "Order.h"

namespace sixchange {

struct PriceLevel {
    Price price{};
    Order* head{};
    Order* tail{};
    Quantity total_quantity{};

    [[nodiscard]] bool empty() const noexcept {
        return head == nullptr;
    }

    void push_back(Order* order) noexcept {
        order->prev = tail;
        order->next = nullptr;

        if (tail != nullptr) {
            tail->next = order;
        }
        else {
            head = order;
        }

        tail = order;
        total_quantity += order->remaining_quantity;
    }

    void remove(Order* order) noexcept {
        if (order->prev != nullptr) {
            order->prev->next = order->next;
        }
        else {
            head = order->next;
        }

        if (order->next != nullptr) {
            order->next->prev = order->prev;
        }
        else {
            tail = order->prev;
        }

        total_quantity -= order->remaining_quantity;
        order->prev = nullptr;
        order->next = nullptr;
    }

    [[nodiscard]] Order* front() const noexcept {
        return head;
    }
};

} // namespace sixchange
