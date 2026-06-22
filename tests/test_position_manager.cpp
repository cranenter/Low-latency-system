#include "trading/position/position_manager.hpp"
#include "trading/risk/pre_trade_risk_engine.hpp"

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

trading::ExecutionReport fill(
    trading::Symbol symbol,
    trading::Side side,
    trading::Price price,
    trading::Quantity quantity)
{
    trading::ExecutionReport report{};
    report.symbol = symbol;
    report.side = side;
    report.last_price = price;
    report.last_quantity = quantity;
    report.exec_type = trading::ExecType::Fill;
    report.order_status = trading::OrderStatus::Filled;
    return report;
}

void test_initial_position_is_zero(int& failures)
{
    trading::PositionManager positions;

    expect_true(positions.current_position(trading::Symbol("AAPL")) == 0, "initial symbol position zero", failures);
    expect_true(positions.position() == 0, "initial aggregate position zero", failures);
    expect_true(positions.average_price(trading::Symbol("AAPL")) == 0, "initial average price zero", failures);
}

void test_buy_fill_increases_position(int& failures)
{
    trading::PositionManager positions;
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10000, 10));

    expect_true(positions.current_position(trading::Symbol("AAPL")) == 10, "buy increases position", failures);
    expect_true(positions.total_bought() == 10, "buy updates bought total", failures);
    expect_true(positions.average_price(trading::Symbol("AAPL")) == 10000, "buy average price", failures);
}

void test_sell_fill_decreases_position(int& failures)
{
    trading::PositionManager positions;
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10000, 10));
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Sell, 10010, 4));

    expect_true(positions.current_position(trading::Symbol("AAPL")) == 6, "sell decreases position", failures);
    expect_true(positions.total_sold() == 4, "sell updates sold total", failures);
    expect_true(positions.realized_pnl(trading::Symbol("AAPL")) == 40, "sell realized pnl", failures);
}

void test_multiple_fills_update_average_price(int& failures)
{
    trading::PositionManager positions;
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10000, 10));
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10020, 10));

    expect_true(positions.current_position(trading::Symbol("AAPL")) == 20, "multiple fills position", failures);
    expect_true(positions.average_price(trading::Symbol("AAPL")) == 10010, "multiple fills average", failures);
}

void test_position_can_be_queried_by_symbol(int& failures)
{
    trading::PositionManager positions;
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10000, 10));
    positions.apply_fill(fill(trading::Symbol("MSFT"), trading::Side::Sell, 25000, 3));

    expect_true(positions.current_position(trading::Symbol("AAPL")) == 10, "aapl position", failures);
    expect_true(positions.current_position(trading::Symbol("MSFT")) == -3, "msft position", failures);
    expect_true(positions.position() == 7, "aggregate position", failures);
}

void test_summary_output(int& failures)
{
    trading::PositionManager positions;
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10000, 10));

    const auto text = positions.summary();
    expect_true(text.find("positions=1") != std::string::npos, "summary count", failures);
    expect_true(text.find("AAPL") != std::string::npos, "summary symbol", failures);
    expect_true(text.find("qty=10") != std::string::npos, "summary quantity", failures);
}

void test_risk_engine_can_use_position_snapshot(int& failures)
{
    trading::PositionManager positions;
    positions.apply_fill(fill(trading::Symbol("AAPL"), trading::Side::Buy, 10000, 95));

    trading::RiskConfig config{};
    config.add_allowed_symbol(trading::Symbol("AAPL"));
    config.max_order_quantity = 100;
    config.max_notional = 1000000;
    config.max_abs_position = 100;
    config.price_band_ticks = 10;

    trading::PreTradeRiskEngine risk(config);
    trading::Order order(
        1,
        trading::Symbol("AAPL"),
        trading::Side::Buy,
        trading::OrderType::Limit,
        trading::TimeInForce::Day,
        10000,
        10,
        1);
    const auto result = risk.validate(order,
        positions.current_position(trading::Symbol("AAPL")),
        trading::RiskMarketState{9995, 10005, 10000},
        100);

    expect_true(!result.accepted, "risk uses position snapshot", failures);
    expect_true(result.reason == trading::RiskRejectReason::PositionLimitExceeded,
        "risk position snapshot reason", failures);
}

} // namespace

int run_position_manager_tests()
{
    int failures = 0;

    test_initial_position_is_zero(failures);
    test_buy_fill_increases_position(failures);
    test_sell_fill_decreases_position(failures);
    test_multiple_fills_update_average_price(failures);
    test_position_can_be_queried_by_symbol(failures);
    test_summary_output(failures);
    test_risk_engine_can_use_position_snapshot(failures);

    return failures;
}

