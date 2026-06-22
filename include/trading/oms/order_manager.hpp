#pragma once

#include "trading/oms/order_state_machine.hpp"

#include <cstddef>
#include <unordered_map>

namespace trading {

class OrderManager {
public:
    OrderManager() = default;

    [[nodiscard]] Order* create_order(
        Symbol symbol,
        Side side,
        Price price,
        Quantity quantity,
        Timestamp timestamp);

    [[nodiscard]] Order* find(ClientOrderId client_order_id) noexcept;
    [[nodiscard]] const Order* find(ClientOrderId client_order_id) const noexcept;

    [[nodiscard]] bool submit(ClientOrderId client_order_id) noexcept;
    [[nodiscard]] bool on_new_ack(ClientOrderId client_order_id, OrderId exchange_order_id = 0) noexcept;
    [[nodiscard]] bool on_new_reject(ClientOrderId client_order_id) noexcept;
    [[nodiscard]] bool on_partial_fill(ClientOrderId client_order_id, Quantity quantity, Price price) noexcept;
    [[nodiscard]] bool on_fill(ClientOrderId client_order_id, Quantity quantity, Price price) noexcept;
    [[nodiscard]] bool request_cancel(ClientOrderId client_order_id) noexcept;
    [[nodiscard]] bool on_cancel_ack(ClientOrderId client_order_id) noexcept;
    [[nodiscard]] bool on_cancel_reject(ClientOrderId client_order_id) noexcept;

    [[nodiscard]] Quantity remaining_quantity(ClientOrderId client_order_id) const noexcept;
    [[nodiscard]] std::size_t active_order_count() const noexcept;

private:
    [[nodiscard]] bool apply_event(
        ClientOrderId client_order_id,
        OrderEvent event,
        Quantity quantity = 0,
        Price price = 0) noexcept;

    ClientOrderId next_client_order_id_{1};
    std::unordered_map<ClientOrderId, Order> orders_;
};

} // namespace trading

