#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace trading {

enum class LatencyStage {
    MarketDataToStrategy,
    StrategyToRisk,
    RiskToGateway,
    GatewayToExecutionReport,
    EndToEnd
};

struct LatencyStats {
    std::size_t count{};
    std::int64_t min_ns{};
    std::int64_t max_ns{};
    std::int64_t average_ns{};
    std::int64_t p50_ns{};
    std::int64_t p95_ns{};
    std::int64_t p99_ns{};
};

class LatencyRecorder {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    LatencyRecorder() = default;

    [[nodiscard]] static TimePoint now() noexcept;
    [[nodiscard]] static const char* stage_name(LatencyStage stage) noexcept;

    // Average latency is not enough for trading systems: rare tail events can
    // dominate slippage and order placement quality. This simple recorder keeps
    // named nanosecond samples and reports p50/p95/p99. Production systems may
    // use TSC, hardware timestamps, kernel-bypass timestamps, or binary tracing
    // to reduce measurement overhead and improve timestamp fidelity.
    void record(LatencyStage stage, TimePoint start, TimePoint end);
    void record(LatencyStage stage, std::int64_t latency_ns);
    void record(const std::string& name, std::int64_t latency_ns);
    void record_sample() noexcept;

    [[nodiscard]] LatencyStats stats(LatencyStage stage) const;
    [[nodiscard]] LatencyStats stats(const std::string& name) const;
    [[nodiscard]] std::size_t sample_count() const noexcept;
    [[nodiscard]] std::string summary() const;

private:
    using SampleMap = std::unordered_map<std::string, std::vector<std::int64_t>>;

    [[nodiscard]] static LatencyStats compute_stats(std::vector<std::int64_t> samples);
    [[nodiscard]] static std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) noexcept;

    SampleMap samples_;
    std::size_t sample_count_{0};
};

} // namespace trading

