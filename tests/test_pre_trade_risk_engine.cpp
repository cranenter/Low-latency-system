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

trading::RiskConfig default_config()
{
    trading::RiskConfig config{};
    config.add_allowed_symbol(trading::Symbol("AAPL"));
    config.max_order_quantity = 100;
    config.max_notional = 1000000;
    config.max_abs_position = 200;
    config.price_band_ticks = 10;
    config.max_orders_per_window = 2;
    config.order_rate_window_ns = 1000;
    return config;
}

trading::Order make_order(
    trading::Symbol symbol = trading::Symbol("AAPL"),
    trading::Side side = trading::Side::Buy,
    trading::Price price = 10005,
    trading::Quantity quantity = 10)
{
    return trading::Order(
        1,
        symbol,
        side,
        trading::OrderType::Limit,
        trading::TimeInForce::Day,
        price,
        quantity,
        100);
}

trading::RiskMarketState default_market()
{
    return trading::RiskMarketState{10000, 10010, 10005};
}

void test_accept_valid_order(int& failures)
{
    trading::PreTradeRiskEngine risk(default_config());

    const auto result = risk.validate(make_order(), 0, default_market(), 100);

    expect_true(result.accepted, "valid order accepted", failures);
    expect_true(result.reason == trading::RiskRejectReason::None, "valid order reason none", failures);
}

void test_reject_disallowed_symbol(int& failures)
{
    trading::PreTradeRiskEngine risk(default_config());

    const auto result = risk.validate(make_order(trading::Symbol("MSFT")), 0, default_market(), 100);

    expect_true(!result.accepted, "disallowed symbol rejected", failures);
    expect_true(result.reason == trading::RiskRejectReason::DisallowedSymbol, "disallowed symbol reason", failures);
}

void test_reject_quantity_too_large(int& failures)
{
    trading::PreTradeRiskEngine risk(default_config());

    const auto result = risk.validate(make_order(trading::Symbol("AAPL"), trading::Side::Buy, 10005, 101),
        0,
        default_market(),
        100);

    expect_true(!result.accepted, "large quantity rejected", failures);
    expect_true(result.reason == trading::RiskRejectReason::QuantityLimitExceeded, "large quantity reason", failures);
}

void test_reject_notional_too_large(int& failures)
{
    auto config = default_config();
    config.max_notional = 50000;
    trading::PreTradeRiskEngine risk(config);

    const auto result = risk.validate(make_order(trading::Symbol("AAPL"), trading::Side::Buy, 10005, 10),
        0,
        default_market(),
        100);

    expect_true(!result.accepted, "large notional rejected", failures);
    expect_true(result.reason == trading::RiskRejectReason::NotionalLimitExceeded, "large notional reason", failures);
}

void test_reject_position_limit_breach(int& failures)
{
    trading::PreTradeRiskEngine risk(default_config());

    const auto result = risk.validate(make_order(trading::Symbol("AAPL"), trading::Side::Buy, 10005, 10),
        195,
        default_market(),
        100);

    expect_true(!result.accepted, "position breach rejected", failures);
    expect_true(result.reason == trading::RiskRejectReason::PositionLimitExceeded, "position breach reason", failures);
}

void test_reject_price_outside_band(int& failures)
{
    trading::PreTradeRiskEngine risk(default_config());

    const auto result = risk.validate(make_order(trading::Symbol("AAPL"), trading::Side::Buy, 10050, 10),
        0,
        default_market(),
        100);

    expect_true(!result.accepted, "price band rejected", failures);
    expect_true(result.reason == trading::RiskRejectReason::PriceBandExceeded, "price band reason", failures);
}

void test_reject_order_rate_breach(int& failures)
{
    trading::PreTradeRiskEngine risk(default_config());

    const auto first = risk.validate(make_order(), 0, default_market(), 100);
    const auto second = risk.validate(make_order(), 0, default_market(), 200);
    const auto third = risk.validate(make_order(), 0, default_market(), 300);
    const auto fourth = risk.validate(make_order(), 0, default_market(), 1200);

    expect_true(first.accepted, "rate first accepted", failures);
    expect_true(second.accepted, "rate second accepted", failures);
    expect_true(!third.accepted, "rate third rejected", failures);
    expect_true(third.reason == trading::RiskRejectReason::OrderRateExceeded, "rate reject reason", failures);
    expect_true(fourth.accepted, "rate resets after window", failures);
}

} // namespace

int run_pre_trade_risk_engine_tests()
{
    int failures = 0;

    test_accept_valid_order(failures);
    test_reject_disallowed_symbol(failures);
    test_reject_quantity_too_large(failures);
    test_reject_notional_too_large(failures);
    test_reject_position_limit_breach(failures);
    test_reject_price_outside_band(failures);
    test_reject_order_rate_breach(failures);

    return failures;
}

