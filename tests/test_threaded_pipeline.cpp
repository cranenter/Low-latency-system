#include "trading/core/Events.hpp"
#include "trading/utils/spsc_queue.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

constexpr std::uint64_t SentinelSequence = UINT64_MAX;

void expect_true(bool condition, const std::string& name, int& failures)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << name << '\n';
    }
}

trading::MarketDataEvent make_event(std::uint64_t sequence)
{
    return trading::MarketDataEvent(
        sequence,
        sequence,
        trading::Symbol("AAPL"),
        sequence,
        sequence,
        trading::MdEventType::Add,
        trading::Side::Buy,
        10000 + static_cast<trading::Price>(sequence),
        1);
}

void test_threaded_spsc_market_data_handoff(int& failures)
{
    trading::SPSCQueue<trading::MarketDataEvent, 64> queue;
    constexpr std::uint64_t event_count = 1000;
    std::uint64_t consumed = 0;
    std::uint64_t sequence_sum = 0;

    std::thread producer([&]() {
        for (std::uint64_t sequence = 1; sequence <= event_count; ++sequence) {
            const auto event = make_event(sequence);
            while (!queue.push(event)) {
                std::this_thread::yield();
            }
        }

        trading::MarketDataEvent sentinel{};
        sentinel.sequence_number = SentinelSequence;
        while (!queue.push(sentinel)) {
            std::this_thread::yield();
        }
    });

    std::thread consumer([&]() {
        trading::MarketDataEvent event{};
        for (;;) {
            if (!queue.pop(event)) {
                std::this_thread::yield();
                continue;
            }
            if (event.sequence_number == SentinelSequence) {
                break;
            }
            ++consumed;
            sequence_sum += event.sequence_number;
        }
    });

    producer.join();
    consumer.join();

    expect_true(consumed == event_count, "threaded handoff consumed count", failures);
    expect_true(sequence_sum == (event_count * (event_count + 1)) / 2,
        "threaded handoff sequence sum", failures);
    expect_true(queue.empty(), "threaded handoff queue empty", failures);
}

} // namespace

int run_threaded_pipeline_tests()
{
    int failures = 0;
    test_threaded_spsc_market_data_handoff(failures);
    return failures;
}

