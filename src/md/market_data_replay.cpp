#include "trading/md/market_data_replay.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace trading {
namespace {

std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parse_u64(const std::string& text, std::uint64_t& out)
{
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(trim(text), &consumed, 10);
        if (consumed != trim(text).size()) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_i64(const std::string& text, std::int64_t& out)
{
    try {
        std::size_t consumed = 0;
        const auto cleaned = trim(text);
        const auto value = std::stoll(cleaned, &consumed, 10);
        if (consumed != cleaned.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_i32(const std::string& text, std::int32_t& out)
{
    std::int64_t value = 0;
    if (!parse_i64(text, value)) {
        return false;
    }
    if (value < 0 || value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    out = static_cast<std::int32_t>(value);
    return true;
}

bool parse_event_type(const std::string& text, MdEventType& out)
{
    const auto value = lower(trim(text));
    if (value == "add") {
        out = MdEventType::Add;
        return true;
    }
    if (value == "modify") {
        out = MdEventType::Modify;
        return true;
    }
    if (value == "cancel") {
        out = MdEventType::Cancel;
        return true;
    }
    if (value == "trade") {
        out = MdEventType::Trade;
        return true;
    }
    return false;
}

bool parse_side(const std::string& text, Side& out)
{
    const auto value = lower(trim(text));
    if (value == "buy" || value == "bid") {
        out = Side::Buy;
        return true;
    }
    if (value == "sell" || value == "ask") {
        out = Side::Sell;
        return true;
    }
    return false;
}

std::vector<std::string> split_csv_line(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);

    while (std::getline(stream, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

bool parse_market_data_line(const std::string& line, MarketDataEvent& event)
{
    const auto fields = split_csv_line(line);
    if (fields.size() != 8) {
        return false;
    }

    std::uint64_t sequence = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t order_id = 0;
    std::int64_t price = 0;
    std::int32_t quantity = 0;
    MdEventType type = MdEventType::Add;
    Side side = Side::Buy;

    if (!parse_u64(fields[0], sequence) || !parse_u64(fields[1], timestamp)
        || fields[2].empty() || !parse_event_type(fields[3], type)
        || !parse_u64(fields[4], order_id) || !parse_side(fields[5], side)
        || !parse_i64(fields[6], price) || !parse_i32(fields[7], quantity)) {
        return false;
    }

    event = MarketDataEvent(
        sequence,
        order_id,
        Symbol(fields[2]),
        timestamp,
        timestamp,
        type,
        side,
        price,
        quantity);
    return true;
}

bool looks_like_header(const std::string& line)
{
    return lower(line).find("sequence") != std::string::npos;
}

} // namespace

bool MarketDataReplay::load_csv(const std::string& csv_path)
{
    clear();

    std::ifstream file(csv_path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::uint64_t previous_sequence = 0;
    bool have_previous_sequence = false;

    while (std::getline(file, line)) {
        const auto cleaned = trim(line);
        if (cleaned.empty()) {
            continue;
        }
        if (events_.empty() && looks_like_header(cleaned)) {
            continue;
        }

        MarketDataEvent event{};
        if (!parse_market_data_line(cleaned, event)) {
            ++invalid_line_count_;
            continue;
        }

        if (have_previous_sequence && event.sequence_number <= previous_sequence) {
            sequence_order_valid_ = false;
        }
        previous_sequence = event.sequence_number;
        have_previous_sequence = true;
        events_.push_back(event);
    }

    return true;
}

void MarketDataReplay::generate_synthetic(std::size_t event_count)
{
    clear();
    events_.reserve(event_count);

    for (std::size_t i = 0; i < event_count; ++i) {
        const auto sequence = static_cast<std::uint64_t>(i + 1);
        const Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        const MdEventType type = (i % 5 == 4) ? MdEventType::Trade : MdEventType::Add;
        const Price price = 10000 + static_cast<Price>(i % 10);
        const Quantity quantity = 10 + static_cast<Quantity>(i % 5);

        events_.emplace_back(
            sequence,
            sequence,
            Symbol("SIM"),
            1000000 + sequence,
            1000000 + sequence,
            type,
            side,
            price,
            quantity);
    }
}

void MarketDataReplay::clear() noexcept
{
    events_.clear();
    invalid_line_count_ = 0;
    sequence_order_valid_ = true;
}

const std::vector<MarketDataEvent>& MarketDataReplay::events() const noexcept
{
    return events_;
}

std::size_t MarketDataReplay::loaded_event_count() const noexcept
{
    return events_.size();
}

bool MarketDataReplay::sequence_order_valid() const noexcept
{
    return sequence_order_valid_;
}

std::size_t MarketDataReplay::invalid_line_count() const noexcept
{
    return invalid_line_count_;
}

} // namespace trading
