#include "trading/oms/order_manager.hpp"

namespace trading {

Order* OrderManager::create_order(
    Symbol symbol,
    Side side,
    Price price,
    Quantity quantity,
    Timestamp timestamp)
{
    const ClientOrderId client_order_id = next_client_order_id_++;
    auto result = orders_.emplace(client_order_id,
        Order(client_order_id, symbol, side, OrderType::Limit, TimeInForce::Day, price, quantity, timestamp));
    return &result.first->second;
}

Order* OrderManager::find(ClientOrderId client_order_id) noexcept
{
    auto it = orders_.find(client_order_id);
    return it == orders_.end() ? nullptr : &it->second;
}

const Order* OrderManager::find(ClientOrderId client_order_id) const noexcept
{
    auto it = orders_.find(client_order_id);
    return it == orders_.end() ? nullptr : &it->second;
}

bool OrderManager::submit(ClientOrderId client_order_id) noexcept
{
    return apply_event(client_order_id, OrderEvent::Submit);
}

bool OrderManager::on_new_ack(ClientOrderId client_order_id, OrderId exchange_order_id) noexcept
{
    Order* order = find(client_order_id);
    if (order == nullptr) {
        return false;
    }
    const bool accepted = OrderStateMachine::apply(*order, OrderEvent::NewAck).accepted;
    if (accepted) {
        order->exchange_order_id = exchange_order_id;
    }
    return accepted;
}

bool OrderManager::on_new_reject(ClientOrderId client_order_id) noexcept
{
    return apply_event(client_order_id, OrderEvent::NewReject);
}

bool OrderManager::on_partial_fill(ClientOrderId client_order_id, Quantity quantity, Price price) noexcept
{
    return apply_event(client_order_id, OrderEvent::PartialFill, quantity, price);
}

bool OrderManager::on_fill(ClientOrderId client_order_id, Quantity quantity, Price price) noexcept
{
    return apply_event(client_order_id, OrderEvent::Fill, quantity, price);
}

bool OrderManager::request_cancel(ClientOrderId client_order_id) noexcept
{
    return apply_event(client_order_id, OrderEvent::CancelRequest);
}

bool OrderManager::on_cancel_ack(ClientOrderId client_order_id) noexcept
{
    return apply_event(client_order_id, OrderEvent::CancelAck);
}

bool OrderManager::on_cancel_reject(ClientOrderId client_order_id) noexcept
{
    return apply_event(client_order_id, OrderEvent::CancelReject);
}

Quantity OrderManager::remaining_quantity(ClientOrderId client_order_id) const noexcept
{
    const Order* order = find(client_order_id);
    return order == nullptr ? 0 : order->remaining_quantity;
}

std::size_t OrderManager::active_order_count() const noexcept
{
    std::size_t count = 0;
    for (const auto& entry : orders_) {
        if (OrderStateMachine::is_active(entry.second.status)) {
            ++count;
        }
    }
    return count;
}

bool OrderManager::apply_event(
    ClientOrderId client_order_id,
    OrderEvent event,
    Quantity quantity,
    Price price) noexcept
{
    Order* order = find(client_order_id);
    if (order == nullptr) {
        return false;
    }
    return OrderStateMachine::apply(*order, event, quantity, price).accepted;
}

} // namespace trading

