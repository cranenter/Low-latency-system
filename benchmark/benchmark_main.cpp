#include "trading/exchange/exchange_simulator.hpp"
#include "trading/execution/execution_report_handler.hpp"
#include "trading/gateway/order_gateway.hpp"
#include "trading/md/order_book.hpp"
#include "trading/metrics/latency_recorder.hpp"
#include "trading/oms/order_manager.hpp"
#include "trading/position/position_manager.hpp"
#include "trading/risk/pre_trade_risk_engine.hpp"
#include "trading/strategy/strategy_simulator.hpp"
#include "trading/utils/object_pool.hpp"
#include "trading/utils/spsc_queue.hpp"

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchObject {
    int id{};
    int value{};

    BenchObject(int object_id, int object_value) noexcept
        : id(object_id)
        , value(object_value)
    {
    }
};

trading::MarketDataEvent make_md_event(std::uint64_t sequence)
{
    const bool bid = sequence % 2 == 0;
    return trading::MarketDataEvent(
        sequence,
        sequence,
        trading::Symbol("SIM"),
        sequence,
        sequence,
        trading::MdEventType::Add,
        bid ? trading::Side::Buy : trading::Side::Sell,
        bid ? 10000 : 10003,
        100);
}

trading::RiskConfig make_risk_config()
{
    trading::RiskConfig config{};
    config.add_allowed_symbol(trading::Symbol("SIM"));
    config.max_order_quantity = 1000;
    config.max_notional = 100'000'000;
    config.max_abs_position = 1'000'000;
    config.price_band_ticks = 1000;
    config.max_orders_per_window = 0;
    config.order_rate_window_ns = 0;
    return config;
}

trading::StrategyConfig make_strategy_config()
{
    trading::StrategyConfig config{};
    config.symbol = trading::Symbol("SIM");
    config.order_quantity = 1;
    config.max_spread = 10;
    config.generate_buy = true;
    config.generate_sell = false;
    return config;
}

double per_second(std::size_t operations, std::int64_t elapsed_ns)
{
    if (elapsed_ns <= 0) {
        return 0.0;
    }
    return static_cast<double>(operations) * 1'000'000'000.0 / static_cast<double>(elapsed_ns);
}

double ns_per_operation(std::size_t operations, std::int64_t elapsed_ns)
{
    if (operations == 0) {
        return 0.0;
    }
    return static_cast<double>(elapsed_ns) / static_cast<double>(operations);
}

void benchmark_queue()
{
    constexpr std::size_t iterations = 2'000'000;
    trading::SPSCQueue<trading::MarketDataEvent, 1024> queue;
    trading::MarketDataEvent out{};

    const auto start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto event = make_md_event(static_cast<std::uint64_t>(i + 1));
        if (!queue.push(event) || !queue.pop(out)) {
            std::cerr << "queue benchmark failed\n";
            return;
        }
    }
    const auto end = Clock::now();
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const std::size_t operations = iterations * 2;

    std::cout << "\n[SPSCQueue]\n";
    std::cout << "events=" << iterations << '\n';
    std::cout << "operations=" << operations << '\n';
    std::cout << "ops_per_sec=" << static_cast<std::uint64_t>(per_second(operations, elapsed_ns)) << '\n';
    std::cout << "avg_ns_per_operation=" << ns_per_operation(operations, elapsed_ns) << '\n';
}

void benchmark_pool()
{
    constexpr std::size_t iterations = 1'000'000;
    trading::ObjectPool<BenchObject, 1024> pool;

    const auto pool_start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        BenchObject* object = pool.allocate(static_cast<int>(i), static_cast<int>(i * 2));
        if (object == nullptr) {
            std::cerr << "pool benchmark allocation failed\n";
            return;
        }
        pool.deallocate(object);
    }
    const auto pool_end = Clock::now();
    const auto pool_elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(pool_end - pool_start).count();

    const auto heap_start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        auto* object = new BenchObject(static_cast<int>(i), static_cast<int>(i * 2));
        delete object;
    }
    const auto heap_end = Clock::now();
    const auto heap_elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(heap_end - heap_start).count();

    std::cout << "\n[ObjectPool]\n";
    std::cout << "allocate_deallocate_cycles=" << iterations << '\n';
    std::cout << "pool_avg_ns_per_cycle=" << ns_per_operation(iterations, pool_elapsed_ns) << '\n';
    std::cout << "new_delete_avg_ns_per_cycle=" << ns_per_operation(iterations, heap_elapsed_ns) << '\n';
}

void benchmark_pipeline()
{
    constexpr std::size_t events = 20'000;
    trading::OrderBook book;
    trading::StrategySimulator strategy(make_strategy_config());
    trading::PreTradeRiskEngine risk(make_risk_config());
    trading::OrderManager orders;
    trading::OrderGateway gateway;
    trading::ExchangeSimulator exchange;
    trading::PositionManager positions;
    trading::ExecutionReportHandler report_handler(orders, positions);
    trading::LatencyRecorder latency;
    std::size_t generated_orders = 0;
    std::size_t reports_processed = 0;

    const auto benchmark_start = Clock::now();
    for (std::size_t i = 0; i < events; ++i) {
        const auto event = make_md_event(static_cast<std::uint64_t>(i + 1));
        if (!book.apply(event)) {
            continue;
        }

        const auto generated = strategy.on_book_update(book, event.receive_timestamp);
        generated_orders += generated.size();

        for (std::size_t j = 0; j < generated.size(); ++j) {
            const auto start = trading::LatencyRecorder::now();
            const auto& order_template = generated[j];
            const trading::RiskMarketState market{book.best_bid(), book.best_ask(), book.best_bid()};
            const auto risk_result = risk.validate(
                order_template,
                positions.current_position(order_template.symbol),
                market,
                event.receive_timestamp);
            if (!risk_result.accepted) {
                continue;
            }

            trading::Order* order = orders.create_order(
                order_template.symbol,
                order_template.side,
                order_template.price,
                order_template.quantity,
                event.receive_timestamp);
            if (order == nullptr || !orders.submit(order->client_order_id)) {
                continue;
            }

            const auto gateway_result = gateway.send_new_order(*order, event.receive_timestamp);
            if (!gateway_result.accepted) {
                continue;
            }

            const auto reports = exchange.process(gateway_result.request);
            for (std::size_t k = 0; k < reports.size(); ++k) {
                const auto handled = report_handler.on_report(reports[k]);
                (void)handled;
                ++reports_processed;
            }
            latency.record(trading::LatencyStage::EndToEnd, start, trading::LatencyRecorder::now());
        }
    }
    const auto benchmark_end = Clock::now();
    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(benchmark_end - benchmark_start).count();
    const auto stats = latency.stats(trading::LatencyStage::EndToEnd);

    std::cout << "\n[Pipeline]\n";
    std::cout << "market_data_events=" << events << '\n';
    std::cout << "events_per_sec=" << static_cast<std::uint64_t>(per_second(events, elapsed_ns)) << '\n';
    std::cout << "strategy_orders=" << generated_orders << '\n';
    std::cout << "execution_reports=" << reports_processed << '\n';
    std::cout << "end_to_end_count=" << stats.count << '\n';
    std::cout << "end_to_end_p50_ns=" << stats.p50_ns << '\n';
    std::cout << "end_to_end_p95_ns=" << stats.p95_ns << '\n';
    std::cout << "end_to_end_p99_ns=" << stats.p99_ns << '\n';
    std::cout << "end_to_end_max_ns=" << stats.max_ns << '\n';
}

} // namespace

int main()
{
    std::cout << "trading_benchmark\n";
    std::cout << "Note: microbenchmarks are approximate and environment-dependent.\n";
    std::cout << std::fixed << std::setprecision(2);

    benchmark_queue();
    benchmark_pool();
    benchmark_pipeline();

    return 0;
}

