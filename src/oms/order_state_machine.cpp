#include "trading/oms/order_state_machine.hpp"

#include <algorithm>
#include <cstdint>

namespace trading {

OrderTransition OrderStateMachine::apply(
    Order& order,
    OrderEvent event,
    Quantity fill_quantity,
    Price fill_price) noexcept
{
    const OrderStatus previous = order.status;
    OrderStatus next = previous;

    switch (event) {
    case OrderEvent::Submit:
        if (previous != OrderStatus::Created) {
            return OrderTransition::reject(previous);
        }
        next = OrderStatus::PendingNew;
        break;
    case OrderEvent::NewAck:
        if (previous != OrderStatus::PendingNew) {
            return OrderTransition::reject(previous);
        }
        next = OrderStatus::Acked;
        break;
    case OrderEvent::NewReject:
        if (previous != OrderStatus::PendingNew) {
            return OrderTransition::reject(previous);
        }
        next = OrderStatus::Rejected;
        break;
    case OrderEvent::PartialFill:
        if (previous != OrderStatus::Acked && previous != OrderStatus::PartiallyFilled
            && previous != OrderStatus::PendingCancel) {
            return OrderTransition::reject(previous);
        }
        if (fill_quantity <= 0 || fill_price <= 0 || order.remaining_quantity <= 0) {
            return OrderTransition::reject(previous);
        }
        apply_fill(order, fill_quantity, fill_price);
        next = order.remaining_quantity == 0 ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
        if (previous == OrderStatus::PendingCancel && next == OrderStatus::PartiallyFilled) {
            next = OrderStatus::PendingCancel;
        }
        break;
    case OrderEvent::Fill:
        if (previous != OrderStatus::Acked && previous != OrderStatus::PartiallyFilled
            && previous != OrderStatus::PendingCancel) {
            return OrderTransition::reject(previous);
        }
        if (fill_quantity <= 0 || fill_price <= 0 || order.remaining_quantity <= 0) {
            return OrderTransition::reject(previous);
        }
        apply_fill(order, fill_quantity, fill_price);
        next = OrderStatus::Filled;
        order.remaining_quantity = 0;
        break;
    case OrderEvent::CancelRequest:
        if (previous != OrderStatus::Acked && previous != OrderStatus::PartiallyFilled) {
            return OrderTransition::reject(previous);
        }
        next = OrderStatus::PendingCancel;
        break;
    case OrderEvent::CancelAck:
        if (previous != OrderStatus::PendingCancel) {
            return OrderTransition::reject(previous);
        }
        next = OrderStatus::Canceled;
        break;
    case OrderEvent::CancelReject:
        if (previous != OrderStatus::PendingCancel) {
            return OrderTransition::reject(previous);
        }
        // Exchange race example: an order may fill while cancel is pending, or
        // the cancel can be rejected because the order is still live. If there
        // are prior fills, return to PartiallyFilled; otherwise return to Acked.
        next = order.filled_quantity > 0 ? OrderStatus::PartiallyFilled : OrderStatus::Acked;
        break;
    }

    order.status = next;
    return OrderTransition{true, previous, next};
}

bool OrderStateMachine::is_terminal(OrderStatus status) noexcept
{
    return status == OrderStatus::Filled || status == OrderStatus::Canceled || status == OrderStatus::Rejected;
}

bool OrderStateMachine::is_active(OrderStatus status) noexcept
{
    return status == OrderStatus::PendingNew || status == OrderStatus::Acked
        || status == OrderStatus::PartiallyFilled || status == OrderStatus::PendingCancel;
}

void OrderStateMachine::apply_fill(Order& order, Quantity fill_quantity, Price fill_price) noexcept
{
    if (fill_quantity <= 0 || fill_price <= 0 || order.remaining_quantity <= 0) {
        return;
    }

    const Quantity applied_quantity = std::min(fill_quantity, order.remaining_quantity);
    const auto previous_notional =
        static_cast<std::int64_t>(order.average_fill_price) * static_cast<std::int64_t>(order.filled_quantity);
    const auto fill_notional =
        static_cast<std::int64_t>(fill_price) * static_cast<std::int64_t>(applied_quantity);

    order.filled_quantity += applied_quantity;
    order.remaining_quantity -= applied_quantity;

    if (order.filled_quantity > 0) {
        order.average_fill_price =
            static_cast<Price>((previous_notional + fill_notional) / order.filled_quantity);
    }
}

} // namespace trading
