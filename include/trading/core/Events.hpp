#pragma once

#include "trading/core/Types.hpp"

namespace trading {

struct MarketDataEvent {
    std::uint64_t sequence_number{};
    OrderId market_data_order_id{};
    Symbol symbol{};
    Timestamp exchange_timestamp{};
    Timestamp receive_timestamp{};
    MdEventType type{MdEventType::Add};
    Side side{Side::Buy};
    Price price{};
    Quantity quantity{};

    MarketDataEvent() noexcept = default;

    MarketDataEvent(
        std::uint64_t event_sequence_number,
        OrderId event_market_data_order_id,
        Symbol event_symbol,
        Timestamp event_exchange_timestamp,
        Timestamp event_receive_timestamp,
        MdEventType event_type,
        Side event_side,
        Price event_price,
        Quantity event_quantity) noexcept
        : sequence_number(event_sequence_number)
        , market_data_order_id(event_market_data_order_id)
        , symbol(event_symbol)
        , exchange_timestamp(event_exchange_timestamp)
        , receive_timestamp(event_receive_timestamp)
        , type(event_type)
        , side(event_side)
        , price(event_price)
        , quantity(event_quantity)
    {
    }

    MarketDataEvent(
        Symbol event_symbol,
        Timestamp event_exchange_timestamp,
        Timestamp event_receive_timestamp,
        MdEventType event_type,
        Side event_side,
        Price event_price,
        Quantity event_quantity) noexcept
        : MarketDataEvent(
            0,
            0,
            event_symbol,
            event_exchange_timestamp,
            event_receive_timestamp,
            event_type,
            event_side,
            event_price,
            event_quantity)
    {
    }
};

struct OrderRequest {
    ClientOrderId client_order_id{};
    Symbol symbol{};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    TimeInForce time_in_force{TimeInForce::Day};
    Price price{};
    Quantity quantity{};
};

struct Order {
    ClientOrderId client_order_id{};
    OrderId exchange_order_id{};
    Symbol symbol{};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    TimeInForce time_in_force{TimeInForce::Day};
    Price price{};
    Quantity quantity{};
    Quantity filled_quantity{};
    Quantity remaining_quantity{};
    Price average_fill_price{};
    OrderStatus status{OrderStatus::Created};
    Timestamp created_timestamp{};

    Order() noexcept = default;

    Order(
        ClientOrderId order_client_id,
        Symbol order_symbol,
        Side order_side,
        OrderType order_type,
        TimeInForce order_time_in_force,
        Price order_price,
        Quantity order_quantity,
        Timestamp order_created_timestamp) noexcept
        : client_order_id(order_client_id)
        , symbol(order_symbol)
        , side(order_side)
        , type(order_type)
        , time_in_force(order_time_in_force)
        , price(order_price)
        , quantity(order_quantity)
        , remaining_quantity(order_quantity)
        , created_timestamp(order_created_timestamp)
    {
    }
};

struct ExecutionReport {
    ClientOrderId client_order_id{};
    OrderId exchange_order_id{};
    ExecType exec_type{ExecType::Ack};
    OrderStatus order_status{OrderStatus::PendingNew};
    RejectReason reject_reason{RejectReason::None};
    Symbol symbol{};
    Side side{Side::Buy};
    Price last_price{};
    Quantity last_quantity{};
    Quantity cumulative_quantity{};
    Quantity leaves_quantity{};
    Timestamp exchange_timestamp{};
    Timestamp receive_timestamp{};
};

struct Trade {
    Symbol symbol{};
    Side side{Side::Buy};
    Price price{};
    Quantity quantity{};
    Timestamp timestamp{};
};

struct Position {
    Symbol symbol{};
    std::int64_t signed_quantity{};
    Quantity total_bought{};
    Quantity total_sold{};

    // Real systems usually track cash, average price, realized/unrealized PnL,
    // account, strategy, clearing firm, and settlement fields.
};

} // namespace trading
