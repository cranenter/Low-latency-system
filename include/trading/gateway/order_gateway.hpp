#pragma once

#include "trading/core/Events.hpp"
#include "trading/oms/order_manager.hpp"

#include <cstdint>

namespace trading {

enum class GatewayRequestType {
    NewOrder,
    CancelOrder
};

enum class GatewayRejectReason {
    None,
    InvalidOrder,
    UnknownOrder,
    MissingRequiredField
};

struct GatewayRequest {
    std::uint64_t gateway_sequence{};
    GatewayRequestType type{GatewayRequestType::NewOrder};
    ClientOrderId client_order_id{};
    OrderId exchange_order_id{};
    Symbol symbol{};
    Side side{Side::Buy};
    OrderType order_type{OrderType::Limit};
    TimeInForce time_in_force{TimeInForce::Day};
    Price price{};
    Quantity quantity{};
    Timestamp send_timestamp{};
};

struct GatewayResult {
    bool accepted{false};
    GatewayRejectReason reject_reason{GatewayRejectReason::None};
    GatewayRequest request{};

    [[nodiscard]] static GatewayResult accept(GatewayRequest request) noexcept
    {
        return GatewayResult{true, GatewayRejectReason::None, request};
    }

    [[nodiscard]] static GatewayResult reject(GatewayRejectReason reason) noexcept
    {
        return GatewayResult{false, reason, GatewayRequest{}};
    }
};

class OrderGateway {
public:
    OrderGateway() = default;

    // Real order gateways manage network sessions, message sequence numbers,
    // reconnects, exchange throttles, heartbeats, recovery, and protocol
    // encoding such as FIX or OUCH. This simulator only models the internal
    // architecture boundary and produces gateway requests for the exchange
    // simulator/execution-report flow.
    [[nodiscard]] GatewayResult send_new_order(const Order& order, Timestamp now) noexcept;
    [[nodiscard]] GatewayResult send_cancel_order(const Order& order, Timestamp now) noexcept;
    [[nodiscard]] GatewayResult send_cancel_order(
        ClientOrderId client_order_id,
        const OrderManager& order_manager,
        Timestamp now) noexcept;

    [[nodiscard]] std::uint64_t next_sequence() noexcept;
    [[nodiscard]] std::uint64_t last_sequence() const noexcept;

private:
    [[nodiscard]] std::uint64_t allocate_sequence() noexcept;
    [[nodiscard]] static bool valid_new_order(const Order& order) noexcept;
    [[nodiscard]] static bool valid_cancel_order(const Order& order) noexcept;

    std::uint64_t next_sequence_{1};
};

} // namespace trading

