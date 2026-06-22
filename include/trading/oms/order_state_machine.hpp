#pragma once

#include "trading/core/Events.hpp"

namespace trading {

enum class OrderEvent {
    Submit,
    NewAck,
    NewReject,
    PartialFill,
    Fill,
    CancelRequest,
    CancelAck,
    CancelReject
};

struct OrderTransition {
    bool accepted{false};
    OrderStatus previous_status{OrderStatus::Created};
    OrderStatus new_status{OrderStatus::Created};

    [[nodiscard]] static OrderTransition reject(OrderStatus status) noexcept
    {
        return OrderTransition{false, status, status};
    }
};

class OrderStateMachine {
public:
    // OMS state correctness matters because downstream risk, position, gateway,
    // and reconciliation logic depend on order lifecycle truth. Invalid
    // transitions often reveal real bugs such as duplicate execution reports,
    // late rejects, or inconsistent cancel/fill races and should be detected.
    [[nodiscard]] static OrderTransition apply(
        Order& order,
        OrderEvent event,
        Quantity fill_quantity = 0,
        Price fill_price = 0) noexcept;

    [[nodiscard]] static bool is_terminal(OrderStatus status) noexcept;
    [[nodiscard]] static bool is_active(OrderStatus status) noexcept;

private:
    static void apply_fill(Order& order, Quantity fill_quantity, Price fill_price) noexcept;
};

} // namespace trading

