#pragma once

#include "trading/oms/order_manager.hpp"
#include "trading/position/PositionManager.hpp"

#include <cstdint>
#include <unordered_set>

namespace trading {

enum class ExecutionHandlerRejectReason {
    None,
    UnknownOrder,
    InvalidReport,
    InvalidTransition,
    DuplicateReport
};

struct ExecutionHandlerResult {
    bool accepted{false};
    ExecutionHandlerRejectReason reason{ExecutionHandlerRejectReason::None};

    [[nodiscard]] static ExecutionHandlerResult accept() noexcept
    {
        return ExecutionHandlerResult{true, ExecutionHandlerRejectReason::None};
    }

    [[nodiscard]] static ExecutionHandlerResult reject(ExecutionHandlerRejectReason reason) noexcept
    {
        return ExecutionHandlerResult{false, reason};
    }
};

class ExecutionReportHandler {
public:
    ExecutionReportHandler(OrderManager& orders, PositionManager& positions) noexcept;

    // Execution report handling is critical: exchange reports are the source of
    // truth for order lifecycle and fills. Bugs here can create false positions,
    // duplicate fills, missed rejects, or orders believed canceled while still
    // live at the venue. Normal rejects/unknown reports are returned as results
    // instead of exceptions so the hot path stays deterministic.
    [[nodiscard]] ExecutionHandlerResult on_report(const ExecutionReport& report) noexcept;

private:
    [[nodiscard]] bool duplicate(const ExecutionReport& report) const;
    void remember(const ExecutionReport& report);
    [[nodiscard]] static std::uint64_t report_key(const ExecutionReport& report) noexcept;

    OrderManager& orders_;
    PositionManager& positions_;
    std::unordered_set<std::uint64_t> seen_reports_;
};

} // namespace trading

