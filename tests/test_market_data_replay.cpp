#include "trading/md/market_data_replay.hpp"

#include <cstdio>
#include <fstream>
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

void write_file(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    file << content;
}

void test_parse_valid_csv(int& failures)
{
    const std::string path = "test_market_data_valid.csv";
    write_file(path,
        "sequence,timestamp,symbol,type,order_id,side,price,quantity\n"
        "1,100,AAPL,Add,11,Buy,10000,50\n"
        "2,101,AAPL,Trade,11,Sell,10001,10\n");

    trading::MarketDataReplay replay;
    expect_true(replay.load_csv(path), "valid csv loads", failures);
    expect_true(replay.loaded_event_count() == 2, "valid csv event count", failures);
    expect_true(replay.invalid_line_count() == 0, "valid csv invalid count", failures);
    expect_true(replay.sequence_order_valid(), "valid csv sequence order", failures);

    std::remove(path.c_str());
}

void test_handle_missing_or_invalid_lines_gracefully(int& failures)
{
    trading::MarketDataReplay missing;
    expect_true(!missing.load_csv("file_that_does_not_exist.csv"), "missing csv returns false", failures);
    expect_true(missing.loaded_event_count() == 0, "missing csv no events", failures);

    const std::string path = "test_market_data_invalid.csv";
    write_file(path,
        "sequence,timestamp,symbol,type,order_id,side,price,quantity\n"
        "1,100,AAPL,Add,11,Buy,10000,50\n"
        "bad,line\n"
        "2,101,AAPL,Unknown,11,Sell,10001,10\n"
        "3,102,AAPL,Cancel,11,Buy,10000,0\n");

    trading::MarketDataReplay replay;
    expect_true(replay.load_csv(path), "invalid csv still loads file", failures);
    expect_true(replay.loaded_event_count() == 2, "invalid csv keeps valid events", failures);
    expect_true(replay.invalid_line_count() == 2, "invalid csv counts bad lines", failures);

    std::remove(path.c_str());
}

void test_generate_synthetic_events(int& failures)
{
    trading::MarketDataReplay replay;
    replay.generate_synthetic(5);

    expect_true(replay.loaded_event_count() == 5, "synthetic event count", failures);
    expect_true(replay.invalid_line_count() == 0, "synthetic invalid count", failures);
    expect_true(replay.sequence_order_valid(), "synthetic sequence order", failures);
    expect_true(replay.events()[0].symbol.view() == "SIM", "synthetic symbol", failures);
}

void test_preserve_sequence_order(int& failures)
{
    const std::string path = "test_market_data_sequence.csv";
    write_file(path,
        "sequence,timestamp,symbol,type,order_id,side,price,quantity\n"
        "1,100,AAPL,Add,11,Buy,10000,50\n"
        "3,101,AAPL,Add,12,Sell,10005,40\n"
        "2,102,AAPL,Trade,12,Sell,10005,10\n");

    trading::MarketDataReplay replay;
    expect_true(replay.load_csv(path), "sequence csv loads", failures);
    expect_true(replay.loaded_event_count() == 3, "sequence csv event count", failures);
    expect_true(!replay.sequence_order_valid(), "sequence disorder detected", failures);
    expect_true(replay.events()[0].sequence_number == 1, "sequence first preserved", failures);
    expect_true(replay.events()[1].sequence_number == 3, "sequence second preserved", failures);
    expect_true(replay.events()[2].sequence_number == 2, "sequence third preserved", failures);

    std::remove(path.c_str());
}

void test_verify_event_fields(int& failures)
{
    const std::string path = "test_market_data_fields.csv";
    write_file(path,
        "sequence,timestamp,symbol,type,order_id,side,price,quantity\n"
        "7,900,MSFT,Modify,77,Sell,25000,12\n");

    trading::MarketDataReplay replay;
    expect_true(replay.load_csv(path), "fields csv loads", failures);

    const auto& event = replay.events().front();
    expect_true(event.sequence_number == 7, "field sequence", failures);
    expect_true(event.exchange_timestamp == 900, "field timestamp", failures);
    expect_true(event.receive_timestamp == 900, "field receive timestamp", failures);
    expect_true(event.symbol.view() == "MSFT", "field symbol", failures);
    expect_true(event.type == trading::MdEventType::Modify, "field type", failures);
    expect_true(event.market_data_order_id == 77, "field order id", failures);
    expect_true(event.side == trading::Side::Sell, "field side", failures);
    expect_true(event.price == 25000, "field price", failures);
    expect_true(event.quantity == 12, "field quantity", failures);

    std::remove(path.c_str());
}

} // namespace

int run_market_data_replay_tests()
{
    int failures = 0;

    test_parse_valid_csv(failures);
    test_handle_missing_or_invalid_lines_gracefully(failures);
    test_generate_synthetic_events(failures);
    test_preserve_sequence_order(failures);
    test_verify_event_fields(failures);

    return failures;
}
