#include "trading/exchange/exchange_simulator.hpp"

#include <algorithm>

namespace trading {

bool ExecutionReportBatch::push(const ExecutionReport& report) noexcept
{
    if (count_ == MaxReports) {
        return false;
    }
    reports_[count_++] = report;
    return true;
}

std::size_t ExecutionReportBatch::size() const noexcept
{
    return count_;
}

bool ExecutionReportBatch::empty() const noexcept
{
    return count_ == 0;
}

const ExecutionReport& ExecutionReportBatch::operator[](std::size_t index) const noexcept
{
    return reports_[index];
}

ExecutionReportBatch ExchangeSimulator::process(const GatewayRequest& request) noexcept
{
    if (request.type == GatewayRequestType::CancelOrder) {
        return process_cancel_order(request);
    }
    return process_new_order(request);
}

const char* ExchangeSimulator::venue_name() const noexcept
{
    return "SIM";
}

bool ExchangeSimulator::has_resting_order(ClientOrderId client_order_id) const noexcept
{
    return resting_index_.find(client_order_id) != resting_index_.end();
}

Quantity ExchangeSimulator::resting_quantity(ClientOrderId client_order_id) const noexcept
{
    const auto it = resting_index_.find(client_order_id);
    if (it == resting_index_.end()) {
        return 0;
    }
    return it->second.iterator->remaining_quantity;
}

ExecutionReportBatch ExchangeSimulator::process_new_order(const GatewayRequest& request) noexcept
{
    ExecutionReportBatch batch;
    if (!valid_new_order(request)) {
        add_reject(batch, request);
        return batch;
    }

    const OrderId exchange_order_id = allocate_exchange_order_id();
    add_ack(batch, request, exchange_order_id);

    Quantity incoming_remaining = request.quantity;
    Quantity incoming_cumulative = 0;

    if (request.side == Side::Buy) {
        while (incoming_remaining > 0 && !asks_.empty() && asks_.begin()->first <= request.price) {
            auto price_it = asks_.begin();
            auto& level = price_it->second;
            auto resting_it = level.begin();
            RestingOrder& resting = *resting_it;
            const Quantity fill_quantity = std::min(incoming_remaining, resting.remaining_quantity);
            incoming_remaining -= fill_quantity;
            incoming_cumulative += fill_quantity;
            resting.filled_quantity += fill_quantity;
            resting.remaining_quantity -= fill_quantity;

            add_fill_report(batch,
                resting.client_order_id,
                resting.exchange_order_id,
                resting.symbol,
                resting.side,
                resting.price,
                fill_quantity,
                resting.filled_quantity,
                resting.remaining_quantity,
                request.send_timestamp);

            add_fill_report(batch,
                request.client_order_id,
                exchange_order_id,
                request.symbol,
                request.side,
                resting.price,
                fill_quantity,
                incoming_cumulative,
                incoming_remaining,
                request.send_timestamp);

            if (resting.remaining_quantity == 0) {
                resting_index_.erase(resting.client_order_id);
                level.erase(resting_it);
                if (level.empty()) {
                    asks_.erase(price_it);
                }
            }
        }
    } else {
        while (incoming_remaining > 0 && !bids_.empty() && bids_.begin()->first >= request.price) {
            auto price_it = bids_.begin();
            auto& level = price_it->second;
            auto resting_it = level.begin();
            RestingOrder& resting = *resting_it;
            const Quantity fill_quantity = std::min(incoming_remaining, resting.remaining_quantity);
            incoming_remaining -= fill_quantity;
            incoming_cumulative += fill_quantity;
            resting.filled_quantity += fill_quantity;
            resting.remaining_quantity -= fill_quantity;

            add_fill_report(batch,
                resting.client_order_id,
                resting.exchange_order_id,
                resting.symbol,
                resting.side,
                resting.price,
                fill_quantity,
                resting.filled_quantity,
                resting.remaining_quantity,
                request.send_timestamp);

            add_fill_report(batch,
                request.client_order_id,
                exchange_order_id,
                request.symbol,
                request.side,
                resting.price,
                fill_quantity,
                incoming_cumulative,
                incoming_remaining,
                request.send_timestamp);

            if (resting.remaining_quantity == 0) {
                resting_index_.erase(resting.client_order_id);
                level.erase(resting_it);
                if (level.empty()) {
                    bids_.erase(price_it);
                }
            }
        }
    }

    if (incoming_remaining > 0) {
        rest_order(request, exchange_order_id, incoming_remaining);
    }

    return batch;
}

ExecutionReportBatch ExchangeSimulator::process_cancel_order(const GatewayRequest& request) noexcept
{
    ExecutionReportBatch batch;
    const auto index_it = resting_index_.find(request.client_order_id);
    if (index_it == resting_index_.end()) {
        add_cancel_reject(batch, request);
        return batch;
    }

    const RestingOrder order = *index_it->second.iterator;
    add_cancel_ack(batch, order, request.send_timestamp);
    erase_resting_order(request.client_order_id);
    return batch;
}

bool ExchangeSimulator::valid_new_order(const GatewayRequest& request) const noexcept
{
    return request.type == GatewayRequestType::NewOrder && request.client_order_id != 0
        && !request.symbol.empty() && request.order_type == OrderType::Limit
        && request.price > 0 && request.quantity > 0
        && resting_index_.find(request.client_order_id) == resting_index_.end();
}

OrderId ExchangeSimulator::allocate_exchange_order_id() noexcept
{
    return next_exchange_order_id_++;
}

void ExchangeSimulator::rest_order(
    const GatewayRequest& request,
    OrderId exchange_order_id,
    Quantity remaining_quantity) noexcept
{
    RestingOrder order{};
    order.client_order_id = request.client_order_id;
    order.exchange_order_id = exchange_order_id;
    order.symbol = request.symbol;
    order.side = request.side;
    order.price = request.price;
    order.filled_quantity = request.quantity - remaining_quantity;
    order.remaining_quantity = remaining_quantity;
    order.arrival_timestamp = request.send_timestamp;

    if (request.side == Side::Buy) {
        auto& level = bids_[request.price];
        level.push_back(order);
        auto it = level.end();
        --it;
        resting_index_[request.client_order_id] = RestingIndex{request.side, request.price, it};
        return;
    }

    auto& level = asks_[request.price];
    level.push_back(order);
    auto it = level.end();
    --it;
    resting_index_[request.client_order_id] = RestingIndex{request.side, request.price, it};
}

void ExchangeSimulator::erase_resting_order(ClientOrderId client_order_id) noexcept
{
    const auto index_it = resting_index_.find(client_order_id);
    if (index_it == resting_index_.end()) {
        return;
    }

    const RestingIndex index = index_it->second;
    if (index.side == Side::Buy) {
        auto level_it = bids_.find(index.price);
        if (level_it != bids_.end()) {
            level_it->second.erase(index.iterator);
            if (level_it->second.empty()) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(index.price);
        if (level_it != asks_.end()) {
            level_it->second.erase(index.iterator);
            if (level_it->second.empty()) {
                asks_.erase(level_it);
            }
        }
    }
    resting_index_.erase(index_it);
}

void ExchangeSimulator::add_ack(
    ExecutionReportBatch& batch,
    const GatewayRequest& request,
    OrderId exchange_order_id) noexcept
{
    ExecutionReport report{};
    report.client_order_id = request.client_order_id;
    report.exchange_order_id = exchange_order_id;
    report.exec_type = ExecType::NewAck;
    report.order_status = OrderStatus::Acked;
    report.symbol = request.symbol;
    report.side = request.side;
    report.leaves_quantity = request.quantity;
    report.exchange_timestamp = request.send_timestamp;
    report.receive_timestamp = request.send_timestamp;
    const bool pushed = batch.push(report);
    (void)pushed;
}

void ExchangeSimulator::add_reject(ExecutionReportBatch& batch, const GatewayRequest& request) noexcept
{
    ExecutionReport report{};
    report.client_order_id = request.client_order_id;
    report.exec_type = ExecType::NewReject;
    report.order_status = OrderStatus::Rejected;
    report.reject_reason = RejectReason::ExchangeReject;
    report.symbol = request.symbol;
    report.side = request.side;
    report.exchange_timestamp = request.send_timestamp;
    report.receive_timestamp = request.send_timestamp;
    const bool pushed = batch.push(report);
    (void)pushed;
}

void ExchangeSimulator::add_cancel_ack(
    ExecutionReportBatch& batch,
    const RestingOrder& order,
    Timestamp now) noexcept
{
    ExecutionReport report{};
    report.client_order_id = order.client_order_id;
    report.exchange_order_id = order.exchange_order_id;
    report.exec_type = ExecType::CancelAck;
    report.order_status = OrderStatus::Canceled;
    report.symbol = order.symbol;
    report.side = order.side;
    report.leaves_quantity = order.remaining_quantity;
    report.exchange_timestamp = now;
    report.receive_timestamp = now;
    const bool pushed = batch.push(report);
    (void)pushed;
}

void ExchangeSimulator::add_cancel_reject(ExecutionReportBatch& batch, const GatewayRequest& request) noexcept
{
    ExecutionReport report{};
    report.client_order_id = request.client_order_id;
    report.exchange_order_id = request.exchange_order_id;
    report.exec_type = ExecType::CancelReject;
    report.order_status = OrderStatus::Rejected;
    report.reject_reason = RejectReason::UnknownOrder;
    report.symbol = request.symbol;
    report.side = request.side;
    report.exchange_timestamp = request.send_timestamp;
    report.receive_timestamp = request.send_timestamp;
    const bool pushed = batch.push(report);
    (void)pushed;
}

void ExchangeSimulator::add_fill_report(
    ExecutionReportBatch& batch,
    ClientOrderId client_order_id,
    OrderId exchange_order_id,
    Symbol symbol,
    Side side,
    Price price,
    Quantity last_quantity,
    Quantity cumulative_quantity,
    Quantity leaves_quantity,
    Timestamp now) noexcept
{
    ExecutionReport report{};
    report.client_order_id = client_order_id;
    report.exchange_order_id = exchange_order_id;
    report.exec_type = leaves_quantity == 0 ? ExecType::Fill : ExecType::PartialFill;
    report.order_status = leaves_quantity == 0 ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
    report.symbol = symbol;
    report.side = side;
    report.last_price = price;
    report.last_quantity = last_quantity;
    report.cumulative_quantity = cumulative_quantity;
    report.leaves_quantity = leaves_quantity;
    report.exchange_timestamp = now;
    report.receive_timestamp = now;
    const bool pushed = batch.push(report);
    (void)pushed;
}

} // namespace trading
