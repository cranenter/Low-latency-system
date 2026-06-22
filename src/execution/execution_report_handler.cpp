#include "trading/execution/execution_report_handler.hpp"

namespace trading {

ExecutionReportHandler::ExecutionReportHandler(OrderManager& orders, PositionManager& positions) noexcept
    : orders_(orders)
    , positions_(positions)
{
}

ExecutionHandlerResult ExecutionReportHandler::on_report(const ExecutionReport& report) noexcept
{
    if (report.client_order_id == 0) {
        return ExecutionHandlerResult::reject(ExecutionHandlerRejectReason::InvalidReport);
    }

    if (orders_.find(report.client_order_id) == nullptr) {
        return ExecutionHandlerResult::reject(ExecutionHandlerRejectReason::UnknownOrder);
    }

    if (duplicate(report)) {
        return ExecutionHandlerResult::reject(ExecutionHandlerRejectReason::DuplicateReport);
    }

    bool updated = false;
    switch (report.exec_type) {
    case ExecType::Ack:
        updated = orders_.on_new_ack(report.client_order_id, report.exchange_order_id);
        break;
    case ExecType::Reject:
        updated = orders_.on_new_reject(report.client_order_id);
        break;
    case ExecType::PartialFill:
        updated = orders_.on_partial_fill(report.client_order_id, report.last_quantity, report.last_price);
        if (updated) {
            positions_.apply_fill(report);
        }
        break;
    case ExecType::Fill:
        updated = orders_.on_fill(report.client_order_id, report.last_quantity, report.last_price);
        if (updated) {
            positions_.apply_fill(report);
        }
        break;
    case ExecType::CancelAck:
        updated = orders_.on_cancel_ack(report.client_order_id);
        break;
    case ExecType::CancelReject:
        updated = orders_.on_cancel_reject(report.client_order_id);
        break;
    }

    if (!updated) {
        return ExecutionHandlerResult::reject(ExecutionHandlerRejectReason::InvalidTransition);
    }

    remember(report);
    return ExecutionHandlerResult::accept();
}

bool ExecutionReportHandler::duplicate(const ExecutionReport& report) const
{
    return seen_reports_.find(report_key(report)) != seen_reports_.end();
}

void ExecutionReportHandler::remember(const ExecutionReport& report)
{
    seen_reports_.insert(report_key(report));
}

std::uint64_t ExecutionReportHandler::report_key(const ExecutionReport& report) noexcept
{
    std::uint64_t key = report.client_order_id;
    key ^= static_cast<std::uint64_t>(report.exchange_order_id + 0x9e3779b97f4a7c15ULL) << 1;
    key ^= static_cast<std::uint64_t>(report.exec_type) << 8;
    key ^= static_cast<std::uint64_t>(report.last_quantity) << 16;
    key ^= static_cast<std::uint64_t>(report.cumulative_quantity) << 32;
    key ^= static_cast<std::uint64_t>(report.leaves_quantity) << 48;
    return key;
}

} // namespace trading

