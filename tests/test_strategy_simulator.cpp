#include "trading/md/order_book.hpp"
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

trading::MarketDataEvent event(
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

trading::StrategySimulator make_strategy(
    trading::Quantity quantity,
    trading::Price max_spread,
    bool buy = true,
    bool sell = false)
{
    trading::StrategyConfig config{};
    config.symbol = trading::Symbol("AAPL");
    config.order_quantity = quantity;
    config.max_spread = max_spread;
    config.generate_buy = buy;
    config.generate_sell = sell;
    return trading::StrategySimulator(config);
}

void add_two_sided_book(trading::OrderBook& book, trading::Price bid, trading::Price ask)
{
    book.apply(event(1, 101, trading::Side::Buy, bid, 10));
    book.apply(event(2, 201, trading::Side::Sell, ask, 10));
}

void test_no_order_when_book_empty(int& failures)
{
    trading::OrderBook book;
    auto strategy = make_strategy(5, 10);

    const auto orders = strategy.on_book_update(book, 123);

    expect_true(orders.empty(), "empty book generates no orders", failures);
}

void test_no_order_when_spread_too_wide(int& failures)
{
    trading::OrderBook book;
    add_two_sided_book(book, 10000, 10020);
    auto strategy = make_strategy(5, 5);

    const auto orders = strategy.on_book_update(book, 123);

    expect_true(orders.empty(), "wide spread generates no orders", failures);
}

void test_generate_order_when_spread_acceptable(int& failures)
{
    trading::OrderBook book;
    add_two_sided_book(book, 10000, 10005);
    auto strategy = make_strategy(5, 5);

    const auto orders = strategy.on_book_update(book, 123);

    expect_true(orders.size() == 1, "acceptable spread generates one order", failures);
}

void test_generated_order_fields(int& failures)
{
    trading::OrderBook book;
    add_two_sided_book(book, 10000, 10005);
    auto strategy = make_strategy(5, 5);

    const auto orders = strategy.on_book_update(book, 123);
    const auto& order = orders[0];

    expect_true(order.symbol.view() == "AAPL", "generated symbol", failures);
    expect_true(order.side == trading::Side::Buy, "generated side", failures);
    expect_true(order.price == 10000, "generated price", failures);
    expect_true(order.quantity == 5, "generated quantity", failures);
    expect_true(order.type == trading::OrderType::Limit, "generated order type", failures);
    expect_true(order.time_in_force == trading::TimeInForce::Day, "generated tif", failures);
    expect_true(order.created_timestamp == 123, "generated timestamp", failures);
}

void test_strategy_respects_configured_quantity(int& failures)
{
    trading::OrderBook book;
    add_two_sided_book(book, 10000, 10001);
    auto strategy = make_strategy(17, 5);

    const auto orders = strategy.on_book_update(book, 456);

    expect_true(orders.size() == 1, "configured quantity order generated", failures);
    expect_true(orders[0].quantity == 17, "configured quantity respected", failures);
}

void test_generate_buy_and_sell_when_configured(int& failures)
{
    trading::OrderBook book;
    add_two_sided_book(book, 10000, 10001);
    auto strategy = make_strategy(3, 5, true, true);

    const auto orders = strategy.on_book_update(book, 789);

    expect_true(orders.size() == 2, "buy and sell generated", failures);
    expect_true(orders[0].side == trading::Side::Buy, "first generated buy", failures);
    expect_true(orders[0].price == 10000, "buy at best bid", failures);
    expect_true(orders[1].side == trading::Side::Sell, "second generated sell", failures);
    expect_true(orders[1].price == 10001, "sell at best ask", failures);
}

} // namespace

int run_strategy_simulator_tests()
{
    int failures = 0;

    test_no_order_when_book_empty(failures);
    test_no_order_when_spread_too_wide(failures);
    test_generate_order_when_spread_acceptable(failures);
    test_generated_order_fields(failures);
    test_strategy_respects_configured_quantity(failures);
    test_generate_buy_and_sell_when_configured(failures);

    return failures;
}

