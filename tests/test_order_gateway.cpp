#include "trading/gateway/order_gateway.hpp"

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

trading::Order make_order()
{
    trading::Order order(
        7,
        trading::Symbol("AAPL"),
        trading::Side::Buy,
        trading::OrderType::Limit,
        trading::TimeInForce::Day,
        10000,
        50,
        100);
    order.status = trading::OrderStatus::Acked;
    order.remaining_quantity = 50;
    order.exchange_order_id = 9001;
    return order;
}

void test_send_valid_new_order(int& failures)
{
    trading::OrderGateway gateway;
    const auto order = make_order();

    const auto result = gateway.send_new_order(order, 1234);

    expect_true(result.accepted, "valid new order accepted", failures);
    expect_true(result.reject_reason == trading::GatewayRejectReason::None, "valid new reject reason none", failures);
    expect_true(result.request.gateway_sequence == 1, "valid new sequence", failures);
    expect_true(result.request.type == trading::GatewayRequestType::NewOrder, "valid new request type", failures);
    expect_true(result.request.client_order_id == 7, "valid new client order id", failures);
    expect_true(result.request.symbol.view() == "AAPL", "valid new symbol", failures);
    expect_true(result.request.price == 10000, "valid new price", failures);
    expect_true(result.request.quantity == 50, "valid new quantity", failures);
    expect_true(result.request.send_timestamp == 1234, "valid new timestamp", failures);
}

void test_reject_invalid_new_order(int& failures)
{
    trading::OrderGateway gateway;
    auto order = make_order();
    order.client_order_id = 0;

    const auto result = gateway.send_new_order(order, 1234);

    expect_true(!result.accepted, "invalid new rejected", failures);
    expect_true(result.reject_reason == trading::GatewayRejectReason::InvalidOrder, "invalid new reason", failures);
    expect_true(gateway.last_sequence() == 0, "invalid new does not allocate sequence", failures);
}

void test_send_cancel_for_known_order(int& failures)
{
    trading::OrderManager manager;
    trading::OrderGateway gateway;
    trading::Order* order = manager.create_order(
        trading::Symbol("AAPL"), trading::Side::Sell, 10010, 25, 100);
    (void)manager.submit(order->client_order_id);
    (void)manager.on_new_ack(order->client_order_id, 7001);

    const auto result = gateway.send_cancel_order(order->client_order_id, manager, 2000);

    expect_true(result.accepted, "known cancel accepted", failures);
    expect_true(result.request.type == trading::GatewayRequestType::CancelOrder, "known cancel type", failures);
    expect_true(result.request.client_order_id == order->client_order_id, "known cancel client id", failures);
    expect_true(result.request.exchange_order_id == 7001, "known cancel exchange id", failures);
    expect_true(result.request.quantity == 25, "known cancel remaining quantity", failures);
}

void test_reject_cancel_for_unknown_order(int& failures)
{
    trading::OrderManager manager;
    trading::OrderGateway gateway;

    const auto result = gateway.send_cancel_order(999, manager, 2000);

    expect_true(!result.accepted, "unknown cancel rejected", failures);
    expect_true(result.reject_reason == trading::GatewayRejectReason::UnknownOrder, "unknown cancel reason", failures);
}

void test_gateway_result_fields_are_correct(int& failures)
{
    trading::OrderGateway gateway;
    const auto order = make_order();

    const auto first = gateway.send_new_order(order, 10);
    const auto second = gateway.send_cancel_order(order, 20);

    expect_true(first.accepted, "first result accepted", failures);
    expect_true(second.accepted, "second result accepted", failures);
    expect_true(first.request.gateway_sequence == 1, "first sequence", failures);
    expect_true(second.request.gateway_sequence == 2, "second sequence", failures);
    expect_true(gateway.last_sequence() == 2, "last sequence", failures);
    expect_true(second.request.send_timestamp == 20, "second timestamp", failures);
}

} // namespace

int run_order_gateway_tests()
{
    int failures = 0;

    test_send_valid_new_order(failures);
    test_reject_invalid_new_order(failures);
    test_send_cancel_for_known_order(failures);
    test_reject_cancel_for_unknown_order(failures);
    test_gateway_result_fields_are_correct(failures);

    return failures;
}

