#include "trading/metrics/latency_recorder.hpp"

#include <algorithm>
#include <sstream>

namespace trading {

LatencyRecorder::TimePoint LatencyRecorder::now() noexcept
{
    return Clock::now();
}

const char* LatencyRecorder::stage_name(LatencyStage stage) noexcept
{
    switch (stage) {
    case LatencyStage::MarketDataToStrategy:
        return "market_data_to_strategy";
    case LatencyStage::StrategyToRisk:
        return "strategy_to_risk";
    case LatencyStage::RiskToGateway:
        return "risk_to_gateway";
    case LatencyStage::GatewayToExecutionReport:
        return "gateway_to_execution_report";
    case LatencyStage::EndToEnd:
        return "end_to_end";
    }
    return "unknown";
}

void LatencyRecorder::record(LatencyStage stage, TimePoint start, TimePoint end)
{
    const auto latency_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    record(stage, latency_ns);
}

void LatencyRecorder::record(LatencyStage stage, std::int64_t latency_ns)
{
    record(stage_name(stage), latency_ns);
}

void LatencyRecorder::record(const std::string& name, std::int64_t latency_ns)
{
    if (latency_ns < 0) {
        return;
    }

    samples_[name].push_back(latency_ns);
    ++sample_count_;
}

void LatencyRecorder::record_sample() noexcept
{
    ++sample_count_;
}

LatencyStats LatencyRecorder::stats(LatencyStage stage) const
{
    return stats(stage_name(stage));
}

LatencyStats LatencyRecorder::stats(const std::string& name) const
{
    const auto it = samples_.find(name);
    if (it == samples_.end()) {
        return LatencyStats{};
    }
    return compute_stats(it->second);
}

std::size_t LatencyRecorder::sample_count() const noexcept
{
    return sample_count_;
}

std::string LatencyRecorder::summary() const
{
    std::ostringstream out;
    out << "latency_samples=" << sample_count_;

    for (const auto& entry : samples_) {
        const auto values = compute_stats(entry.second);
        out << "\n  " << entry.first
            << " count=" << values.count
            << " min_ns=" << values.min_ns
            << " max_ns=" << values.max_ns
            << " avg_ns=" << values.average_ns
            << " p50_ns=" << values.p50_ns
            << " p95_ns=" << values.p95_ns
            << " p99_ns=" << values.p99_ns;
    }

    return out.str();
}

LatencyStats LatencyRecorder::compute_stats(std::vector<std::int64_t> samples)
{
    if (samples.empty()) {
        return LatencyStats{};
    }

    std::sort(samples.begin(), samples.end());

    std::int64_t sum = 0;
    for (const auto sample : samples) {
        sum += sample;
    }

    LatencyStats result{};
    result.count = samples.size();
    result.min_ns = samples.front();
    result.max_ns = samples.back();
    result.average_ns = sum / static_cast<std::int64_t>(samples.size());
    result.p50_ns = percentile(samples, 0.50);
    result.p95_ns = percentile(samples, 0.95);
    result.p99_ns = percentile(samples, 0.99);
    return result;
}

std::int64_t LatencyRecorder::percentile(const std::vector<std::int64_t>& sorted, double p) noexcept
{
    if (sorted.empty()) {
        return 0;
    }

    const auto index = static_cast<std::size_t>((sorted.size() - 1) * p);
    return sorted[index];
}

} // namespace trading

