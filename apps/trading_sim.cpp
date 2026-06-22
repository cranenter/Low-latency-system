#include "trading/exchange/exchange_simulator.hpp"
#include "trading/execution/execution_report_handler.hpp"
#include "trading/gateway/order_gateway.hpp"
#include "trading/md/market_data_replay.hpp"
#include "trading/md/order_book.hpp"
#include "trading/metrics/latency_recorder.hpp"
#include "trading/oms/order_manager.hpp"
#include "trading/position/position_manager.hpp"
#include "trading/risk/pre_trade_risk_engine.hpp"
#include "trading/strategy/strategy_simulator.hpp"
#include "trading/utils/spsc_queue.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

constexpr std::uint64_t SentinelSequence = UINT64_MAX;

trading::RiskConfig make_risk_config();
trading::StrategyConfig make_strategy_config();

struct SimStats {
    std::size_t market_data_events{};
    std::size_t strategy_orders_generated{};
    std::size_t risk_accepted_orders{};
    std::size_t risk_rejected_orders{};
    std::size_t gateway_sent_orders{};
    std::size_t exchange_execution_reports{};
    std::size_t fills{};
};

struct PipelineContext {
    trading::OrderBook book;
    trading::StrategySimulator strategy{make_strategy_config()};
    trading::PreTradeRiskEngine risk{make_risk_config()};
    trading::OrderManager orders;
    trading::OrderGateway gateway;
    trading::ExchangeSimulator exchange;
    trading::PositionManager positions;
    trading::ExecutionReportHandler report_handler{orders, positions};
    trading::LatencyRecorder latency;
    SimStats stats;
};

trading::RiskConfig make_risk_config()
{
    trading::RiskConfig config{};
    config.add_allowed_symbol(trading::Symbol("AAPL"));
    config.add_allowed_symbol(trading::Symbol("SIM"));
    config.max_order_quantity = 100;
    config.max_notional = 2'000'000;
    config.max_abs_position = 500;
    config.price_band_ticks = 100;
    config.max_orders_per_window = 1000;
    config.order_rate_window_ns = 1'000'000'000;
    return config;
}

trading::StrategyConfig make_strategy_config()
{
    trading::StrategyConfig config{};
    config.symbol = trading::Symbol("AAPL");
    config.order_quantity = 10;
    config.max_spread = 10;
    config.generate_buy = true;
    config.generate_sell = false;
    return config;
}

bool is_sentinel(const trading::MarketDataEvent& event)
{
    return event.sequence_number == SentinelSequence;
}

void process_execution_reports(
    const trading::ExecutionReportBatch& reports,
    trading::ExecutionReportHandler& handler,
    SimStats& stats)
{
    for (std::size_t i = 0; i < reports.size(); ++i) {
        const auto& report = reports[i];
        ++stats.exchange_execution_reports;
        if (report.exec_type == trading::ExecType::PartialFill || report.exec_type == trading::ExecType::Fill) {
            ++stats.fills;
        }

        const auto handled = handler.on_report(report);
        (void)handled;
    }
}

void process_market_data_event(PipelineContext& context, const trading::MarketDataEvent& md_event)
{
    ++context.stats.market_data_events;
    const bool applied = context.book.apply(md_event);
    if (!applied) {
        return;
    }

    const auto strategy_start = trading::LatencyRecorder::now();
    const auto generated_orders = context.strategy.on_book_update(context.book, md_event.receive_timestamp);
    const auto strategy_end = trading::LatencyRecorder::now();
    context.latency.record(trading::LatencyStage::MarketDataToStrategy, strategy_start, strategy_end);
    context.stats.strategy_orders_generated += generated_orders.size();

    for (std::size_t i = 0; i < generated_orders.size(); ++i) {
        const auto order_start = trading::LatencyRecorder::now();
        const auto& generated = generated_orders[i];
        const trading::RiskMarketState market{
            context.book.best_bid(),
            context.book.best_ask(),
            context.book.has_bid() ? context.book.best_bid() : generated.price};

        const auto risk_start = trading::LatencyRecorder::now();
        const auto risk_result = context.risk.validate(
            generated,
            context.positions.current_position(generated.symbol),
            market,
            md_event.receive_timestamp);
        const auto risk_end = trading::LatencyRecorder::now();
        context.latency.record(trading::LatencyStage::StrategyToRisk, risk_start, risk_end);

        if (!risk_result.accepted) {
            ++context.stats.risk_rejected_orders;
            continue;
        }
        ++context.stats.risk_accepted_orders;

        trading::Order* order = context.orders.create_order(
            generated.symbol,
            generated.side,
            generated.price,
            generated.quantity,
            md_event.receive_timestamp);
        if (order == nullptr || !context.orders.submit(order->client_order_id)) {
            continue;
        }

        const auto gateway_start = trading::LatencyRecorder::now();
        const auto gateway_result = context.gateway.send_new_order(*order, md_event.receive_timestamp);
        const auto gateway_end = trading::LatencyRecorder::now();
        context.latency.record(trading::LatencyStage::RiskToGateway, gateway_start, gateway_end);
        if (!gateway_result.accepted) {
            continue;
        }
        ++context.stats.gateway_sent_orders;

        const auto exchange_start = trading::LatencyRecorder::now();
        const auto reports = context.exchange.process(gateway_result.request);
        const auto exchange_end = trading::LatencyRecorder::now();
        context.latency.record(trading::LatencyStage::GatewayToExecutionReport, exchange_start, exchange_end);
        process_execution_reports(reports, context.report_handler, context.stats);
        const auto order_end = trading::LatencyRecorder::now();
        context.latency.record(trading::LatencyStage::EndToEnd, order_start, order_end);
    }
}

trading::MarketDataReplay load_replay()
{
    trading::MarketDataReplay replay;
    if (!replay.load_csv("data/market_data.csv") || replay.loaded_event_count() == 0) {
        replay.generate_synthetic(20);
    }
    return replay;
}

void run_single_threaded(const trading::MarketDataReplay& replay, PipelineContext& context)
{
    for (const auto& event : replay.events()) {
        process_market_data_event(context, event);
    }
}

void run_threaded(const trading::MarketDataReplay& replay, PipelineContext& context)
{
    // This demo uses SPSC because there is exactly one replay producer thread
    // and exactly one engine consumer thread. Real trading systems may place
    // queues between more stages, shard by symbol, and pin threads to cores.
    // This mode is intentionally limited: it demonstrates queue handoff without
    // turning the whole simulator into a complex asynchronous system.
    trading::SPSCQueue<trading::MarketDataEvent, 1024> queue;
    std::atomic<bool> producer_done{false};

    std::thread producer([&]() {
        for (const auto& event : replay.events()) {
            while (!queue.push(event)) {
                std::this_thread::yield();
            }
        }

        trading::MarketDataEvent sentinel{};
        sentinel.sequence_number = SentinelSequence;
        while (!queue.push(sentinel)) {
            std::this_thread::yield();
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        trading::MarketDataEvent event{};
        for (;;) {
            if (!queue.pop(event)) {
                if (producer_done.load(std::memory_order_acquire) && queue.empty()) {
                    continue;
                }
                std::this_thread::yield();
                continue;
            }

            if (is_sentinel(event)) {
                break;
            }
            process_market_data_event(context, event);
        }
    });

    producer.join();
    consumer.join();
}

void print_summary(const std::string& mode, const PipelineContext& context)
{
    const auto& stats = context.stats;

    std::cout << "trading_sim summary\n";
    std::cout << "mode=" << mode << '\n';
    std::cout << "market_data_events=" << stats.market_data_events << '\n';
    std::cout << "strategy_orders_generated=" << stats.strategy_orders_generated << '\n';
    std::cout << "risk_accepted_orders=" << stats.risk_accepted_orders << '\n';
    std::cout << "risk_rejected_orders=" << stats.risk_rejected_orders << '\n';
    std::cout << "gateway_sent_orders=" << stats.gateway_sent_orders << '\n';
    std::cout << "exchange_execution_reports=" << stats.exchange_execution_reports << '\n';
    std::cout << "fills=" << stats.fills << '\n';
    std::cout << "final_positions=" << context.positions.summary() << '\n';
    std::cout << context.latency.summary() << '\n';
}

std::string parse_mode(int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--mode") {
            return argv[i + 1];
        }
    }
    return "single";
}

} // namespace

int main(int argc, char** argv)
{
    const std::string mode = parse_mode(argc, argv);
    if (mode != "single" && mode != "threaded") {
        std::cerr << "usage: trading_sim [--mode single|threaded]\n";
        return 1;
    }

    const auto replay = load_replay();
    PipelineContext context;
    if (mode == "threaded") {
        run_threaded(replay, context);
    } else {
        run_single_threaded(replay, context);
    }
    print_summary(mode, context);

    return 0;
}
