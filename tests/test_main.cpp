#include "trading/core/Events.hpp"
#include "trading/risk/RiskEngine.hpp"
#include "trading/utils/ObjectPool.hpp"
#include "trading/utils/SPSCQueue.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int run_spsc_queue_tests();
int run_core_types_tests();
int run_object_pool_tests();
int run_market_data_replay_tests();
int run_order_book_tests();
int run_strategy_simulator_tests();
int run_pre_trade_risk_engine_tests();
int run_order_state_machine_tests();
int run_order_manager_tests();
int run_order_gateway_tests();
int run_exchange_simulator_tests();
int run_end_to_end_pipeline_tests();
int run_execution_report_handler_tests();
int run_position_manager_tests();
int run_latency_recorder_tests();
int run_threaded_pipeline_tests();

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << name << '\n';
    }
}

void test_queue_placeholder()
{
    trading::SPSCQueue<int, 4> queue;
    int value = 0;

    expect_true(queue.try_push(42), "queue push succeeds");
    expect_true(queue.try_pop(value), "queue pop succeeds");
    expect_true(value == 42, "queue preserves value");
}

void test_object_pool_placeholder()
{
    trading::ObjectPool<int, 1> pool;
    int* value = pool.allocate();

    expect_true(value != nullptr, "pool allocates first object");
    expect_true(pool.allocate() == nullptr, "pool reports exhaustion");
}

void test_risk_placeholder()
{
    trading::RiskEngine risk;
    trading::OrderRequest request{};
    request.price = 100;
    request.quantity = 1;

    expect_true(risk.allow(request), "risk allows positive price and quantity");
}

void test_core_symbol()
{
    const trading::Symbol symbol("AAPL");

    expect_true(!symbol.empty(), "symbol is not empty");
    expect_true(symbol.view() == "AAPL", "symbol stores expected value");

    const trading::Symbol truncated("ABCDEFGHIJKLMNOPQRST");
    expect_true(truncated.view().size() == trading::Symbol::MaxLength - 1, "symbol truncates to fixed storage");
}

void test_core_enums()
{
    trading::Side side = trading::Side::Buy;
    trading::OrderType order_type = trading::OrderType::Limit;
    trading::TimeInForce tif = trading::TimeInForce::IOC;
    trading::OrderStatus status = trading::OrderStatus::PendingNew;
    trading::RejectReason reject_reason = trading::RejectReason::None;
    trading::MdEventType md_type = trading::MdEventType::Trade;
    trading::ExecType exec_type = trading::ExecType::Fill;

    expect_true(side == trading::Side::Buy, "side enum");
    expect_true(order_type == trading::OrderType::Limit, "order type enum");
    expect_true(tif == trading::TimeInForce::IOC, "time in force enum");
    expect_true(status == trading::OrderStatus::PendingNew, "order status enum");
    expect_true(reject_reason == trading::RejectReason::None, "reject reason enum");
    expect_true(md_type == trading::MdEventType::Trade, "market data event enum");
    expect_true(exec_type == trading::ExecType::Fill, "execution type enum");
}

void test_market_data_event_construction()
{
    trading::MarketDataEvent event(
        trading::Symbol("MSFT"),
        100,
        110,
        trading::MdEventType::Add,
        trading::Side::Sell,
        25000,
        10);

    expect_true(event.symbol.view() == "MSFT", "market data symbol");
    expect_true(event.exchange_timestamp == 100, "market data exchange timestamp");
    expect_true(event.receive_timestamp == 110, "market data receive timestamp");
    expect_true(event.type == trading::MdEventType::Add, "market data type");
    expect_true(event.side == trading::Side::Sell, "market data side");
    expect_true(event.price == 25000, "market data price");
    expect_true(event.quantity == 10, "market data quantity");
}

void test_order_construction()
{
    trading::Order order(
        7,
        trading::Symbol("ES"),
        trading::Side::Buy,
        trading::OrderType::Limit,
        trading::TimeInForce::Day,
        500000,
        2,
        1234);

    expect_true(order.client_order_id == 7, "order client id");
    expect_true(order.exchange_order_id == 0, "order default exchange id");
    expect_true(order.symbol.view() == "ES", "order symbol");
    expect_true(order.side == trading::Side::Buy, "order side");
    expect_true(order.type == trading::OrderType::Limit, "order type");
    expect_true(order.time_in_force == trading::TimeInForce::Day, "order tif");
    expect_true(order.price == 500000, "order price");
    expect_true(order.quantity == 2, "order quantity");
    expect_true(order.filled_quantity == 0, "order default filled quantity");
    expect_true(order.status == trading::OrderStatus::New, "order default status");
    expect_true(order.created_timestamp == 1234, "order created timestamp");
}

void test_execution_report_trade_and_position()
{
    trading::ExecutionReport report{};
    report.client_order_id = 9;
    report.exchange_order_id = 77;
    report.exec_type = trading::ExecType::PartialFill;
    report.order_status = trading::OrderStatus::PartiallyFilled;
    report.symbol = trading::Symbol("NQ");
    report.side = trading::Side::Sell;
    report.last_price = 1800000;
    report.last_quantity = 1;
    report.cumulative_quantity = 1;
    report.leaves_quantity = 3;

    expect_true(report.client_order_id == 9, "execution report client id");
    expect_true(report.exchange_order_id == 77, "execution report exchange id");
    expect_true(report.exec_type == trading::ExecType::PartialFill, "execution report type");
    expect_true(report.order_status == trading::OrderStatus::PartiallyFilled, "execution report order status");
    expect_true(report.symbol.view() == "NQ", "execution report symbol");
    expect_true(report.side == trading::Side::Sell, "execution report side");
    expect_true(report.last_price == 1800000, "execution report last price");
    expect_true(report.last_quantity == 1, "execution report last quantity");
    expect_true(report.cumulative_quantity == 1, "execution report cumulative quantity");
    expect_true(report.leaves_quantity == 3, "execution report leaves quantity");

    trading::Trade trade{trading::Symbol("NQ"), trading::Side::Sell, 1800000, 1, 2000};
    expect_true(trade.symbol.view() == "NQ", "trade symbol");
    expect_true(trade.side == trading::Side::Sell, "trade side");
    expect_true(trade.price == 1800000, "trade price");
    expect_true(trade.quantity == 1, "trade quantity");
    expect_true(trade.timestamp == 2000, "trade timestamp");

    trading::Position position{};
    position.symbol = trading::Symbol("NQ");
    position.signed_quantity = -1;
    position.total_sold = 1;

    expect_true(position.symbol.view() == "NQ", "position symbol");
    expect_true(position.signed_quantity == -1, "position signed quantity");
    expect_true(position.total_bought == 0, "position default bought quantity");
    expect_true(position.total_sold == 1, "position sold quantity");
}

} // namespace

int main()
{
    test_queue_placeholder();
    test_object_pool_placeholder();
    test_risk_placeholder();
    test_core_symbol();
    test_core_enums();
    test_market_data_event_construction();
    test_order_construction();
    test_execution_report_trade_and_position();
    failures += run_core_types_tests();
    failures += run_market_data_replay_tests();
    failures += run_order_book_tests();
    failures += run_order_state_machine_tests();
    failures += run_order_manager_tests();
    failures += run_order_gateway_tests();
    failures += run_exchange_simulator_tests();
    failures += run_end_to_end_pipeline_tests();
    failures += run_execution_report_handler_tests();
    failures += run_position_manager_tests();
    failures += run_latency_recorder_tests();
    failures += run_object_pool_tests();
    failures += run_pre_trade_risk_engine_tests();
    failures += run_spsc_queue_tests();
    failures += run_strategy_simulator_tests();
    failures += run_threaded_pipeline_tests();

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "All tests passed\n";
    return EXIT_SUCCESS;
}
