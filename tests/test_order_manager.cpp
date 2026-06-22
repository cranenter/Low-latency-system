#include "trading/oms/order_manager.hpp"

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

void ignore_result(bool value)
{
    (void)value;
}

void test_manager_stores_and_finds_order(int& failures)
{
    trading::OrderManager manager;
    trading::Order* order = manager.create_order(
        trading::Symbol("AAPL"), trading::Side::Buy, 10000, 100, 1);

    expect_true(order != nullptr, "manager create order", failures);
    expect_true(order->client_order_id == 1, "manager client id", failures);
    expect_true(manager.find(order->client_order_id) == order, "manager find order", failures);
}

void test_manager_lifecycle_and_active_count(int& failures)
{
    trading::OrderManager manager;
    trading::Order* order = manager.create_order(
        trading::Symbol("AAPL"), trading::Side::Buy, 10000, 100, 1);

    expect_true(manager.submit(order->client_order_id), "manager submit", failures);
    expect_true(manager.active_order_count() == 1, "manager pending active count", failures);
    expect_true(manager.on_new_ack(order->client_order_id, 9001), "manager ack", failures);
    expect_true(order->exchange_order_id == 9001, "manager exchange id", failures);
    expect_true(manager.active_order_count() == 1, "manager ack active count", failures);
    expect_true(manager.on_fill(order->client_order_id, 100, 10000), "manager fill", failures);
    expect_true(manager.active_order_count() == 0, "manager terminal active count", failures);
}

void test_manager_fill_accounting(int& failures)
{
    trading::OrderManager manager;
    trading::Order* order = manager.create_order(
        trading::Symbol("AAPL"), trading::Side::Buy, 10000, 100, 1);

    ignore_result(manager.submit(order->client_order_id));
    ignore_result(manager.on_new_ack(order->client_order_id));
    ignore_result(manager.on_partial_fill(order->client_order_id, 40, 10000));
    ignore_result(manager.on_fill(order->client_order_id, 60, 10020));

    expect_true(order->filled_quantity == 100, "manager cumulative filled", failures);
    expect_true(manager.remaining_quantity(order->client_order_id) == 0, "manager remaining quantity", failures);
    expect_true(order->average_fill_price == 10012, "manager average fill price", failures);
    expect_true(order->status == trading::OrderStatus::Filled, "manager filled state", failures);
}

void test_manager_cancel_reject(int& failures)
{
    trading::OrderManager manager;
    trading::Order* order = manager.create_order(
        trading::Symbol("AAPL"), trading::Side::Sell, 10010, 100, 1);

    ignore_result(manager.submit(order->client_order_id));
    ignore_result(manager.on_new_ack(order->client_order_id));
    expect_true(manager.request_cancel(order->client_order_id), "manager cancel request", failures);
    expect_true(order->status == trading::OrderStatus::PendingCancel, "manager pending cancel", failures);
    expect_true(manager.on_cancel_reject(order->client_order_id), "manager cancel reject", failures);
    expect_true(order->status == trading::OrderStatus::Acked, "manager cancel reject acked", failures);
}

void test_manager_partial_fill_then_cancel(int& failures)
{
    trading::OrderManager manager;
    trading::Order* order = manager.create_order(
        trading::Symbol("AAPL"), trading::Side::Sell, 10010, 100, 1);

    ignore_result(manager.submit(order->client_order_id));
    ignore_result(manager.on_new_ack(order->client_order_id));
    ignore_result(manager.on_partial_fill(order->client_order_id, 30, 10010));
    ignore_result(manager.request_cancel(order->client_order_id));
    ignore_result(manager.on_cancel_ack(order->client_order_id));

    expect_true(order->status == trading::OrderStatus::Canceled, "manager partial cancel state", failures);
    expect_true(order->filled_quantity == 30, "manager partial cancel filled", failures);
    expect_true(order->remaining_quantity == 70, "manager partial cancel remaining", failures);
}

void test_manager_invalid_unknown_order(int& failures)
{
    trading::OrderManager manager;

    expect_true(!manager.submit(999), "manager unknown submit rejected", failures);
    expect_true(!manager.on_fill(999, 1, 100), "manager unknown fill rejected", failures);
    expect_true(manager.remaining_quantity(999) == 0, "manager unknown remaining zero", failures);
}

} // namespace

int run_order_manager_tests()
{
    int failures = 0;

    test_manager_stores_and_finds_order(failures);
    test_manager_lifecycle_and_active_count(failures);
    test_manager_fill_accounting(failures);
    test_manager_cancel_reject(failures);
    test_manager_partial_fill_then_cancel(failures);
    test_manager_invalid_unknown_order(failures);

    return failures;
}
