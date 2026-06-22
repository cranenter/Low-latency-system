#pragma once

#include "trading/gateway/order_gateway.hpp"

#include <array>
#include <cstddef>
#include <list>
#include <map>
#include <unordered_map>

namespace trading {

class ExecutionReportBatch {
public:
    static constexpr std::size_t MaxReports = 16;

    [[nodiscard]] bool push(const ExecutionReport& report) noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] const ExecutionReport& operator[](std::size_t index) const noexcept;

private:
    std::array<ExecutionReport, MaxReports> reports_{};
    std::size_t count_{0};
};

class ExchangeSimulator {
public:
    ExchangeSimulator() = default;

    // This is an exchange simulator for exercising OMS and pipeline behavior,
    // not a production exchange. Real venues have complex order types,
    // self-match rules, auctions, hidden liquidity, protocol sequencing, and
    // deterministic matching engines. This model keeps price-time priority
    // simple and testable.
    [[nodiscard]] ExecutionReportBatch process(const GatewayRequest& request) noexcept;
    [[nodiscard]] const char* venue_name() const noexcept;
    [[nodiscard]] bool has_resting_order(ClientOrderId client_order_id) const noexcept;
    [[nodiscard]] Quantity resting_quantity(ClientOrderId client_order_id) const noexcept;

private:
    struct RestingOrder {
        ClientOrderId client_order_id{};
        OrderId exchange_order_id{};
        Symbol symbol{};
        Side side{Side::Buy};
        Price price{};
        Quantity filled_quantity{};
        Quantity remaining_quantity{};
        Timestamp arrival_timestamp{};
    };

    using OrderList = std::list<RestingOrder>;
    using BidBook = std::map<Price, OrderList, std::greater<Price>>;
    using AskBook = std::map<Price, OrderList, std::less<Price>>;

    struct RestingIndex {
        Side side{Side::Buy};
        Price price{};
        OrderList::iterator iterator{};
    };

    [[nodiscard]] ExecutionReportBatch process_new_order(const GatewayRequest& request) noexcept;
    [[nodiscard]] ExecutionReportBatch process_cancel_order(const GatewayRequest& request) noexcept;
    [[nodiscard]] bool valid_new_order(const GatewayRequest& request) const noexcept;
    [[nodiscard]] OrderId allocate_exchange_order_id() noexcept;

    void rest_order(const GatewayRequest& request, OrderId exchange_order_id, Quantity remaining_quantity) noexcept;
    void erase_resting_order(ClientOrderId client_order_id) noexcept;

    void add_ack(ExecutionReportBatch& batch, const GatewayRequest& request, OrderId exchange_order_id) noexcept;
    void add_reject(ExecutionReportBatch& batch, const GatewayRequest& request) noexcept;
    void add_cancel_ack(ExecutionReportBatch& batch, const RestingOrder& order, Timestamp now) noexcept;
    void add_cancel_reject(ExecutionReportBatch& batch, const GatewayRequest& request) noexcept;
    void add_fill_report(
        ExecutionReportBatch& batch,
        ClientOrderId client_order_id,
        OrderId exchange_order_id,
        Symbol symbol,
        Side side,
        Price price,
        Quantity last_quantity,
        Quantity cumulative_quantity,
        Quantity leaves_quantity,
        Timestamp now) noexcept;

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<ClientOrderId, RestingIndex> resting_index_;
    OrderId next_exchange_order_id_{1};
};

} // namespace trading
