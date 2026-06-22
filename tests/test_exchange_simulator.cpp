#include "trading/exchange/exchange_simulator.hpp"

#include <iostream>
#include <string>

namespace {

void expect_true(bool condition, const std::string& name, int& failures)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << name << '\n';
    }
}

trading::GatewayRequest new_order(
    trading::ClientOrderId client_order_id,
    trading::Side side,
    trading::Price price,
    trading::Quantity quantity,
    trading::Timestamp timestamp = 100)
{
    trading::GatewayRequest request{};
    request.gateway_sequence = client_order_id;
    request.type = trading::GatewayRequestType::NewOrder;
    request.client_order_id = client_order_id;
    request.symbol = trading::Symbol("AAPL");
    request.side = side;
    request.order_type = trading::OrderType::Limit;
    request.time_in_force = trading::TimeInForce::Day;
    request.price = price;
    request.quantity = quantity;
    request.send_timestamp = timestamp;
    return request;
}

trading::GatewayRequest cancel_order(trading::ClientOrderId client_order_id, trading::Timestamp timestamp = 200)
{
    trading::GatewayRequest request{};
    request.type = trading::GatewayRequestType::CancelOrder;
    request.client_order_id = client_order_id;
    request.symbol = trading::Symbol("AAPL");
    request.send_timestamp = timestamp;
    return request;
}

void test_new_order_receives_ack(int& failures)
{
    trading::ExchangeSimulator exchange;
    const auto reports = exchange.process(new_order(1, trading::Side::Buy, 10000, 10));

    expect_true(reports.size() == 1, "new order ack report count", failures);
    expect_true(reports[0].exec_type == trading::ExecType::NewAck, "new order ack type", failures);
    expect_true(reports[0].order_status == trading::OrderStatus::Acked, "new order ack status", failures);
    expect_true(reports[0].client_order_id == 1, "new order ack client id", failures);
}

void test_invalid_order_receives_reject(int& failures)
{
    trading::ExchangeSimulator exchange;
    auto request = new_order(1, trading::Side::Buy, 0, 10);
    const auto reports = exchange.process(request);

    expect_true(reports.size() == 1, "invalid order report count", failures);
    expect_true(reports[0].exec_type == trading::ExecType::NewReject, "invalid order reject type", failures);
    expect_true(reports[0].order_status == trading::OrderStatus::Rejected, "invalid order status", failures);
}

void test_buy_order_rests_when_no_sell_exists(int& failures)
{
    trading::ExchangeSimulator exchange;
    const auto reports = exchange.process(new_order(1, trading::Side::Buy, 10000, 10));

    expect_true(reports.size() == 1, "resting buy ack only", failures);
    expect_true(exchange.has_resting_order(1), "buy rests", failures);
    expect_true(exchange.resting_quantity(1) == 10, "buy resting quantity", failures);
}

void test_sell_order_rests_when_no_buy_exists(int& failures)
{
    trading::ExchangeSimulator exchange;
    const auto reports = exchange.process(new_order(1, trading::Side::Sell, 10010, 10));

    expect_true(reports.size() == 1, "resting sell ack only", failures);
    expect_true(exchange.has_resting_order(1), "sell rests", failures);
    expect_true(exchange.resting_quantity(1) == 10, "sell resting quantity", failures);
}

void test_buy_crosses_resting_ask_and_receives_fill(int& failures)
{
    trading::ExchangeSimulator exchange;
    (void)exchange.process(new_order(1, trading::Side::Sell, 10005, 10));

    const auto reports = exchange.process(new_order(2, trading::Side::Buy, 10005, 10));

    expect_true(reports.size() == 3, "buy cross report count", failures);
    expect_true(reports[0].exec_type == trading::ExecType::NewAck, "buy cross ack", failures);
    expect_true(reports[1].client_order_id == 1, "buy cross resting order report", failures);
    expect_true(reports[1].exec_type == trading::ExecType::Fill, "buy cross resting fill", failures);
    expect_true(reports[2].client_order_id == 2, "buy cross incoming report", failures);
    expect_true(reports[2].exec_type == trading::ExecType::Fill, "buy cross incoming fill", failures);
    expect_true(!exchange.has_resting_order(1), "buy cross removes ask", failures);
}

void test_sell_crosses_resting_bid_and_receives_fill(int& failures)
{
    trading::ExchangeSimulator exchange;
    (void)exchange.process(new_order(1, trading::Side::Buy, 10000, 10));

    const auto reports = exchange.process(new_order(2, trading::Side::Sell, 10000, 10));

    expect_true(reports.size() == 3, "sell cross report count", failures);
    expect_true(reports[1].client_order_id == 1, "sell cross resting order report", failures);
    expect_true(reports[1].exec_type == trading::ExecType::Fill, "sell cross resting fill", failures);
    expect_true(reports[2].client_order_id == 2, "sell cross incoming report", failures);
    expect_true(reports[2].exec_type == trading::ExecType::Fill, "sell cross incoming fill", failures);
}

void test_partial_fill(int& failures)
{
    trading::ExchangeSimulator exchange;
    (void)exchange.process(new_order(1, trading::Side::Sell, 10005, 100));

    const auto reports = exchange.process(new_order(2, trading::Side::Buy, 10005, 40));

    expect_true(reports.size() == 3, "partial fill report count", failures);
    expect_true(reports[1].client_order_id == 1, "partial resting id", failures);
    expect_true(reports[1].exec_type == trading::ExecType::PartialFill, "partial resting report", failures);
    expect_true(reports[1].leaves_quantity == 60, "partial resting leaves", failures);
    expect_true(reports[2].client_order_id == 2, "partial incoming id", failures);
    expect_true(reports[2].exec_type == trading::ExecType::Fill, "partial incoming fill", failures);
    expect_true(exchange.resting_quantity(1) == 60, "partial resting quantity", failures);
}

void test_cancel_resting_order_receives_cancel_ack(int& failures)
{
    trading::ExchangeSimulator exchange;
    (void)exchange.process(new_order(1, trading::Side::Buy, 10000, 10));

    const auto reports = exchange.process(cancel_order(1));

    expect_true(reports.size() == 1, "cancel ack report count", failures);
    expect_true(reports[0].exec_type == trading::ExecType::CancelAck, "cancel ack type", failures);
    expect_true(reports[0].order_status == trading::OrderStatus::Canceled, "cancel ack status", failures);
    expect_true(!exchange.has_resting_order(1), "cancel removes resting order", failures);
}

void test_cancel_unknown_order_receives_cancel_reject(int& failures)
{
    trading::ExchangeSimulator exchange;
    const auto reports = exchange.process(cancel_order(404));

    expect_true(reports.size() == 1, "cancel reject report count", failures);
    expect_true(reports[0].exec_type == trading::ExecType::CancelReject, "cancel reject type", failures);
    expect_true(reports[0].reject_reason == trading::RejectReason::UnknownOrder, "cancel reject reason", failures);
}

void test_duplicate_resting_client_order_id_rejected(int& failures)
{
    trading::ExchangeSimulator exchange;
    (void)exchange.process(new_order(1, trading::Side::Buy, 10000, 10));

    const auto reports = exchange.process(new_order(1, trading::Side::Buy, 9999, 5));

    expect_true(reports.size() == 1, "duplicate client id report count", failures);
    expect_true(reports[0].exec_type == trading::ExecType::NewReject, "duplicate client id reject", failures);
    expect_true(exchange.resting_quantity(1) == 10, "duplicate client id preserves original order", failures);
}

void test_fifo_at_same_price(int& failures)
{
    trading::ExchangeSimulator exchange;
    (void)exchange.process(new_order(1, trading::Side::Sell, 10005, 10, 100));
    (void)exchange.process(new_order(2, trading::Side::Sell, 10005, 10, 101));

    const auto reports = exchange.process(new_order(3, trading::Side::Buy, 10005, 10, 102));

    expect_true(reports.size() == 3, "fifo report count", failures);
    expect_true(reports[1].client_order_id == 1, "fifo fills first resting order", failures);
    expect_true(!exchange.has_resting_order(1), "fifo first removed", failures);
    expect_true(exchange.has_resting_order(2), "fifo second remains", failures);

    const auto cancel_first = exchange.process(cancel_order(1));
    const auto cancel_second = exchange.process(cancel_order(2));
    expect_true(cancel_first[0].exec_type == trading::ExecType::CancelReject, "fifo first cancel reject", failures);
    expect_true(cancel_second[0].exec_type == trading::ExecType::CancelAck, "fifo second cancel ack", failures);
}

} // namespace

int run_exchange_simulator_tests()
{
    int failures = 0;

    test_new_order_receives_ack(failures);
    test_invalid_order_receives_reject(failures);
    test_buy_order_rests_when_no_sell_exists(failures);
    test_sell_order_rests_when_no_buy_exists(failures);
    test_buy_crosses_resting_ask_and_receives_fill(failures);
    test_sell_crosses_resting_bid_and_receives_fill(failures);
    test_partial_fill(failures);
    test_cancel_resting_order_receives_cancel_ack(failures);
    test_cancel_unknown_order_receives_cancel_reject(failures);
    test_duplicate_resting_client_order_id_rejected(failures);
    test_fifo_at_same_price(failures);

    return failures;
}
