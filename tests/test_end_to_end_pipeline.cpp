#include "trading/exchange/exchange_simulator.hpp"
#include "trading/execution/execution_report_handler.hpp"
#include "trading/gateway/order_gateway.hpp"
#include "trading/md/order_book.hpp"
#include "trading/metrics/latency_recorder.hpp"
#include "trading/oms/order_manager.hpp"
#include "trading/position/position_manager.hpp"
#include "trading/risk/pre_trade_risk_engine.hpp"
#include "trading/strategy/strategy_simulator.hpp"

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

trading::MarketDataEvent md_event(
    std::uint64_t sequence,
    trading::OrderId order_id,
    trading::Side side,
    trading::Price price,
    trading::Quantity quantity)
{
    return trading::MarketDataEvent(
        sequence,
        order_id,
        trading::Symbol("AAPL"),
        1000 + sequence,
        1000 + sequence,
        trading::MdEventType::Add,
        side,
        price,
        quantity);
}

trading::RiskConfig risk_config()
{
    trading::RiskConfig config{};
    config.add_allowed_symbol(trading::Symbol("AAPL"));
    config.max_order_quantity = 100;
    config.max_notional = 1'000'000;
    config.max_abs_position = 1000;
    config.price_band_ticks = 100;
    return config;
}

trading::StrategyConfig strategy_config()
{
    trading::StrategyConfig config{};
    config.symbol = trading::Symbol("AAPL");
    config.order_quantity = 10;
    config.max_spread = 10;
    config.generate_buy = true;
    config.generate_sell = false;
    return config;
}

trading::GatewayRequest resting_ask_request()
{
    trading::GatewayRequest request{};
    request.gateway_sequence = 1;
    request.type = trading::GatewayRequestType::NewOrder;
    request.client_order_id = 900;
    request.symbol = trading::Symbol("AAPL");
    request.side = trading::Side::Sell;
    request.order_type = trading::OrderType::Limit;
    request.time_in_force = trading::TimeInForce::Day;
    request.price = 10000;
    request.quantity = 10;
    request.send_timestamp = 1;
    return request;
}

void test_single_threaded_pipeline_generates_and_fills_order(int& failures)
{
    trading::OrderBook book;
    trading::StrategySimulator strategy(strategy_config());
    trading::PreTradeRiskEngine risk(risk_config());
    trading::OrderManager orders;
    trading::OrderGateway gateway;
    trading::ExchangeSimulator exchange;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(orders, positions);
    trading::LatencyRecorder latency;

    const auto seeded_reports = exchange.process(resting_ask_request());
    expect_true(seeded_reports.size() == 1, "e2e seed resting ask ack", failures);

    expect_true(book.apply(md_event(1, 101, trading::Side::Buy, 10000, 100)),
        "e2e apply bid", failures);
    expect_true(book.apply(md_event(2, 102, trading::Side::Sell, 10005, 100)),
        "e2e apply ask", failures);

    const auto start = trading::LatencyRecorder::now();
    const auto generated = strategy.on_book_update(book, 2);
    expect_true(generated.size() == 1, "e2e strategy generated one order", failures);

    const auto& generated_order = generated[0];
    const trading::RiskMarketState market{book.best_bid(), book.best_ask(), book.best_bid()};
    const auto risk_result = risk.validate(
        generated_order,
        positions.current_position(generated_order.symbol),
        market,
        2);
    expect_true(risk_result.accepted, "e2e risk accepted", failures);

    trading::Order* order = orders.create_order(
        generated_order.symbol,
        generated_order.side,
        generated_order.price,
        generated_order.quantity,
        2);
    expect_true(order != nullptr, "e2e order created", failures);
    expect_true(order != nullptr && orders.submit(order->client_order_id), "e2e order submitted", failures);

    const auto gateway_result = gateway.send_new_order(*order, 2);
    expect_true(gateway_result.accepted, "e2e gateway accepted", failures);

    const auto reports = exchange.process(gateway_result.request);
    std::size_t accepted_reports = 0;
    std::size_t fills = 0;
    for (std::size_t i = 0; i < reports.size(); ++i) {
        if (reports[i].exec_type == trading::ExecType::Fill
            || reports[i].exec_type == trading::ExecType::PartialFill) {
            ++fills;
        }
        const auto result = handler.on_report(reports[i]);
        if (result.accepted) {
            ++accepted_reports;
        }
    }
    latency.record(trading::LatencyStage::EndToEnd, start, trading::LatencyRecorder::now());

    expect_true(reports.size() == 3, "e2e exchange emitted ack plus fills", failures);
    expect_true(accepted_reports == 2, "e2e handler accepted incoming ack/fill", failures);
    expect_true(fills == 2, "e2e exchange emitted two fill reports", failures);
    expect_true(order->status == trading::OrderStatus::Filled, "e2e order filled", failures);
    expect_true(order->remaining_quantity == 0, "e2e remaining zero", failures);
    expect_true(positions.current_position(trading::Symbol("AAPL")) == 10, "e2e final position", failures);
    expect_true(latency.stats(trading::LatencyStage::EndToEnd).count == 1, "e2e latency recorded", failures);
}

void test_single_threaded_pipeline_risk_reject_path(int& failures)
{
    trading::OrderBook book;
    trading::StrategySimulator strategy(strategy_config());
    auto config = risk_config();
    config.max_order_quantity = 1;
    trading::PreTradeRiskEngine risk(config);
    trading::PositionManager positions;

    expect_true(book.apply(md_event(1, 101, trading::Side::Buy, 10000, 100)),
        "e2e reject apply bid", failures);
    expect_true(book.apply(md_event(2, 102, trading::Side::Sell, 10005, 100)),
        "e2e reject apply ask", failures);

    const auto generated = strategy.on_book_update(book, 2);
    const trading::RiskMarketState market{book.best_bid(), book.best_ask(), book.best_bid()};
    const auto risk_result = risk.validate(
        generated[0],
        positions.current_position(generated[0].symbol),
        market,
        2);

    expect_true(!risk_result.accepted, "e2e risk reject", failures);
    expect_true(risk_result.reason == trading::RiskRejectReason::QuantityLimitExceeded,
        "e2e risk reject reason", failures);
}

} // namespace

int run_end_to_end_pipeline_tests()
{
    int failures = 0;

    test_single_threaded_pipeline_generates_and_fills_order(failures);
    test_single_threaded_pipeline_risk_reject_path(failures);

    return failures;
}

