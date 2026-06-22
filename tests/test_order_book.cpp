#include "trading/md/order_book.hpp"

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
    trading::MdEventType type,
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
        type,
        side,
        price,
        quantity);
}

void test_add_bid_updates_best_bid(int& failures)
{
    trading::OrderBook book;
    expect_true(book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Buy, 10000, 50)),
        "add bid accepted", failures);
    expect_true(book.has_bid(), "has bid", failures);
    expect_true(book.best_bid() == 10000, "best bid price", failures);
    expect_true(book.quantity_at(trading::Side::Buy, 10000) == 50, "bid quantity", failures);
}

void test_add_ask_updates_best_ask(int& failures)
{
    trading::OrderBook book;
    expect_true(book.apply(event(1, trading::MdEventType::Add, 201, trading::Side::Sell, 10005, 40)),
        "add ask accepted", failures);
    expect_true(book.has_ask(), "has ask", failures);
    expect_true(book.best_ask() == 10005, "best ask price", failures);
    expect_true(book.quantity_at(trading::Side::Sell, 10005) == 40, "ask quantity", failures);
}

void test_multiple_bid_levels_choose_highest_bid(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Buy, 10000, 50));
    book.apply(event(2, trading::MdEventType::Add, 102, trading::Side::Buy, 9990, 20));
    book.apply(event(3, trading::MdEventType::Add, 103, trading::Side::Buy, 10010, 15));

    expect_true(book.best_bid() == 10010, "highest bid chosen", failures);
}

void test_multiple_ask_levels_choose_lowest_ask(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 201, trading::Side::Sell, 10010, 50));
    book.apply(event(2, trading::MdEventType::Add, 202, trading::Side::Sell, 10020, 20));
    book.apply(event(3, trading::MdEventType::Add, 203, trading::Side::Sell, 10005, 15));

    expect_true(book.best_ask() == 10005, "lowest ask chosen", failures);
}

void test_modify_order_changes_quantity_or_price(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Buy, 10000, 50));

    expect_true(book.apply(event(2, trading::MdEventType::Modify, 101, trading::Side::Buy, 10010, 25)),
        "modify accepted", failures);
    expect_true(book.best_bid() == 10010, "modify price", failures);
    expect_true(book.quantity_at(trading::Side::Buy, 10000) == 0, "modify old level removed", failures);
    expect_true(book.quantity_at(trading::Side::Buy, 10010) == 25, "modify new quantity", failures);
}

void test_cancel_order_removes_it(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Buy, 10000, 50));

    expect_true(book.apply(event(2, trading::MdEventType::Cancel, 101, trading::Side::Buy, 10000, 0)),
        "cancel accepted", failures);
    expect_true(!book.has_order(101), "cancel removes order lookup", failures);
    expect_true(!book.has_bid(), "cancel removes bid level", failures);
}

void test_trade_reduces_quantity(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Sell, 10005, 50));

    expect_true(book.apply(event(2, trading::MdEventType::Trade, 101, trading::Side::Sell, 10005, 20)),
        "trade accepted", failures);
    expect_true(book.has_order(101), "partial trade keeps order", failures);
    expect_true(book.quantity_at(trading::Side::Sell, 10005) == 30, "trade reduces level quantity", failures);
}

void test_trade_fully_removes_order(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Sell, 10005, 50));

    expect_true(book.apply(event(2, trading::MdEventType::Trade, 101, trading::Side::Sell, 10005, 50)),
        "full trade accepted", failures);
    expect_true(!book.has_order(101), "full trade removes order", failures);
    expect_true(!book.has_ask(), "full trade removes ask level", failures);
}

void test_spread_calculation(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Buy, 10000, 50));
    book.apply(event(2, trading::MdEventType::Add, 201, trading::Side::Sell, 10005, 40));

    expect_true(book.spread() == 5, "spread calculation", failures);
}

void test_top_levels(int& failures)
{
    trading::OrderBook book;
    book.apply(event(1, trading::MdEventType::Add, 101, trading::Side::Buy, 10000, 10));
    book.apply(event(2, trading::MdEventType::Add, 102, trading::Side::Buy, 10010, 20));
    book.apply(event(3, trading::MdEventType::Add, 103, trading::Side::Buy, 9990, 30));
    book.apply(event(4, trading::MdEventType::Add, 104, trading::Side::Buy, 10010, 5));

    const auto levels = book.top_levels(trading::Side::Buy, 2);
    expect_true(levels.size() == 2, "top levels size", failures);
    expect_true(levels[0].price == 10010, "top levels first price", failures);
    expect_true(levels[0].quantity == 25, "top levels first aggregated quantity", failures);
    expect_true(levels[1].price == 10000, "top levels second price", failures);
    expect_true(levels[1].quantity == 10, "top levels second quantity", failures);
}

} // namespace

int run_order_book_tests()
{
    int failures = 0;

    test_add_bid_updates_best_bid(failures);
    test_add_ask_updates_best_ask(failures);
    test_multiple_bid_levels_choose_highest_bid(failures);
    test_multiple_ask_levels_choose_lowest_ask(failures);
    test_modify_order_changes_quantity_or_price(failures);
    test_cancel_order_removes_it(failures);
    test_trade_reduces_quantity(failures);
    test_trade_fully_removes_order(failures);
    test_spread_calculation(failures);
    test_top_levels(failures);

    return failures;
}

