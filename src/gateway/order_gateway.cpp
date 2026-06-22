#include "trading/gateway/order_gateway.hpp"

namespace trading {

GatewayResult OrderGateway::send_new_order(const Order& order, Timestamp now) noexcept
{
    if (!valid_new_order(order)) {
        return GatewayResult::reject(GatewayRejectReason::InvalidOrder);
    }

    GatewayRequest request{};
    request.gateway_sequence = allocate_sequence();
    request.type = GatewayRequestType::NewOrder;
    request.client_order_id = order.client_order_id;
    request.exchange_order_id = order.exchange_order_id;
    request.symbol = order.symbol;
    request.side = order.side;
    request.order_type = order.type;
    request.time_in_force = order.time_in_force;
    request.price = order.price;
    request.quantity = order.quantity;
    request.send_timestamp = now;

    return GatewayResult::accept(request);
}

GatewayResult OrderGateway::send_cancel_order(const Order& order, Timestamp now) noexcept
{
    if (!valid_cancel_order(order)) {
        return GatewayResult::reject(GatewayRejectReason::InvalidOrder);
    }

    GatewayRequest request{};
    request.gateway_sequence = allocate_sequence();
    request.type = GatewayRequestType::CancelOrder;
    request.client_order_id = order.client_order_id;
    request.exchange_order_id = order.exchange_order_id;
    request.symbol = order.symbol;
    request.side = order.side;
    request.order_type = order.type;
    request.time_in_force = order.time_in_force;
    request.price = order.price;
    request.quantity = order.remaining_quantity;
    request.send_timestamp = now;

    return GatewayResult::accept(request);
}

GatewayResult OrderGateway::send_cancel_order(
    ClientOrderId client_order_id,
    const OrderManager& order_manager,
    Timestamp now) noexcept
{
    const Order* order = order_manager.find(client_order_id);
    if (order == nullptr) {
        return GatewayResult::reject(GatewayRejectReason::UnknownOrder);
    }
    return send_cancel_order(*order, now);
}

std::uint64_t OrderGateway::next_sequence() noexcept
{
    return allocate_sequence();
}

std::uint64_t OrderGateway::last_sequence() const noexcept
{
    return next_sequence_ == 1 ? 0 : next_sequence_ - 1;
}

std::uint64_t OrderGateway::allocate_sequence() noexcept
{
    return next_sequence_++;
}

bool OrderGateway::valid_new_order(const Order& order) noexcept
{
    return order.client_order_id != 0 && !order.symbol.empty() && order.price > 0
        && order.quantity > 0;
}

bool OrderGateway::valid_cancel_order(const Order& order) noexcept
{
    return order.client_order_id != 0 && !order.symbol.empty()
        && (order.status == OrderStatus::Acked || order.status == OrderStatus::PartiallyFilled
            || order.status == OrderStatus::PendingCancel)
        && order.remaining_quantity > 0;
}

} // namespace trading

