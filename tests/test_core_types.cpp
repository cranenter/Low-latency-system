#include "trading/core/Events.hpp"

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

void test_symbol_fixed_storage_and_equality(int& failures)
{
    const trading::Symbol empty;
    const trading::Symbol aapl("AAPL");
    const trading::Symbol aapl_copy("AAPL");
    const trading::Symbol msft("MSFT");
    const trading::Symbol long_symbol("ABCDEFGHIJKLMNOPQRST");

    expect_true(empty.empty(), "core empty symbol", failures);
    expect_true(aapl.view() == "AAPL", "core symbol view", failures);
    expect_true(aapl == aapl_copy, "core symbol equality", failures);
    expect_true(aapl != msft, "core symbol inequality", failures);
    expect_true(long_symbol.view().size() == trading::Symbol::MaxLength - 1,
        "core symbol truncation", failures);
}

void test_order_status_aliases(int& failures)
{
    expect_true(trading::OrderStatus::New == trading::OrderStatus::Created,
        "core new created alias", failures);
    expect_true(trading::OrderStatus::Live == trading::OrderStatus::Acked,
        "core live acked alias", failures);
    expect_true(trading::OrderStatus::Cancelled == trading::OrderStatus::Canceled,
        "core cancelled canceled alias", failures);
}

void test_exec_type_aliases(int& failures)
{
    expect_true(trading::ExecType::Ack == trading::ExecType::NewAck,
        "core ack newack alias", failures);
    expect_true(trading::ExecType::Reject == trading::ExecType::NewReject,
        "core reject newreject alias", failures);
}

void test_order_defaults_and_remaining_quantity(int& failures)
{
    trading::Order order(
        42,
        trading::Symbol("AAPL"),
        trading::Side::Buy,
        trading::OrderType::Limit,
        trading::TimeInForce::Day,
        10000,
        25,
        500);

    expect_true(order.client_order_id == 42, "core order client id", failures);
    expect_true(order.status == trading::OrderStatus::Created, "core order created status", failures);
    expect_true(order.quantity == 25, "core order quantity", failures);
    expect_true(order.remaining_quantity == 25, "core order remaining defaults to quantity", failures);
    expect_true(order.filled_quantity == 0, "core order filled default", failures);
    expect_true(order.average_fill_price == 0, "core order average price default", failures);
}

void test_market_data_sequence_fields(int& failures)
{
    trading::MarketDataEvent event(
        7,
        7001,
        trading::Symbol("AAPL"),
        100,
        110,
        trading::MdEventType::Cancel,
        trading::Side::Sell,
        10005,
        0);

    expect_true(event.sequence_number == 7, "core md sequence", failures);
    expect_true(event.market_data_order_id == 7001, "core md order id", failures);
    expect_true(event.type == trading::MdEventType::Cancel, "core md cancel type", failures);
}

} // namespace

int run_core_types_tests()
{
    int failures = 0;

    test_symbol_fixed_storage_and_equality(failures);
    test_order_status_aliases(failures);
    test_exec_type_aliases(failures);
    test_order_defaults_and_remaining_quantity(failures);
    test_market_data_sequence_fields(failures);

    return failures;
}

