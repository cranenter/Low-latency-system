#include "trading/metrics/latency_recorder.hpp"

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

void test_empty_recorder_behavior(int& failures)
{
    trading::LatencyRecorder recorder;
    const auto stats = recorder.stats(trading::LatencyStage::EndToEnd);

    expect_true(recorder.sample_count() == 0, "empty sample count", failures);
    expect_true(stats.count == 0, "empty stats count", failures);
    expect_true(stats.min_ns == 0, "empty min", failures);
    expect_true(stats.p99_ns == 0, "empty p99", failures);
}

void test_one_sample(int& failures)
{
    trading::LatencyRecorder recorder;
    recorder.record(trading::LatencyStage::EndToEnd, 100);
    const auto stats = recorder.stats(trading::LatencyStage::EndToEnd);

    expect_true(stats.count == 1, "one count", failures);
    expect_true(stats.min_ns == 100, "one min", failures);
    expect_true(stats.max_ns == 100, "one max", failures);
    expect_true(stats.average_ns == 100, "one average", failures);
    expect_true(stats.p50_ns == 100, "one p50", failures);
    expect_true(stats.p95_ns == 100, "one p95", failures);
    expect_true(stats.p99_ns == 100, "one p99", failures);
}

void test_multiple_samples(int& failures)
{
    trading::LatencyRecorder recorder;
    recorder.record(trading::LatencyStage::StrategyToRisk, 100);
    recorder.record(trading::LatencyStage::StrategyToRisk, 300);
    recorder.record(trading::LatencyStage::StrategyToRisk, 200);
    const auto stats = recorder.stats(trading::LatencyStage::StrategyToRisk);

    expect_true(stats.count == 3, "multiple count", failures);
    expect_true(stats.min_ns == 100, "multiple min", failures);
    expect_true(stats.max_ns == 300, "multiple max", failures);
    expect_true(stats.average_ns == 200, "multiple average", failures);
    expect_true(stats.p50_ns == 200, "multiple p50", failures);
}

void test_percentile_calculation(int& failures)
{
    trading::LatencyRecorder recorder;
    for (int i = 1; i <= 100; ++i) {
        recorder.record(trading::LatencyStage::GatewayToExecutionReport, i);
    }
    const auto stats = recorder.stats(trading::LatencyStage::GatewayToExecutionReport);

    expect_true(stats.p50_ns == 50, "percentile p50", failures);
    expect_true(stats.p95_ns == 95, "percentile p95", failures);
    expect_true(stats.p99_ns == 99, "percentile p99", failures);
}

void test_named_latency_groups(int& failures)
{
    trading::LatencyRecorder recorder;
    recorder.record("custom_group", 10);
    recorder.record("custom_group", 30);
    recorder.record(trading::LatencyStage::EndToEnd, 100);

    const auto custom = recorder.stats("custom_group");
    const auto end_to_end = recorder.stats(trading::LatencyStage::EndToEnd);
    const auto text = recorder.summary();

    expect_true(recorder.sample_count() == 3, "named total sample count", failures);
    expect_true(custom.count == 2, "named group count", failures);
    expect_true(custom.average_ns == 20, "named group average", failures);
    expect_true(end_to_end.count == 1, "stage group count", failures);
    expect_true(text.find("custom_group") != std::string::npos, "summary contains custom group", failures);
    expect_true(text.find("end_to_end") != std::string::npos, "summary contains stage group", failures);
}

} // namespace

int run_latency_recorder_tests()
{
    int failures = 0;

    test_empty_recorder_behavior(failures);
    test_one_sample(failures);
    test_multiple_samples(failures);
    test_percentile_calculation(failures);
    test_named_latency_groups(failures);

    return failures;
}

