#include "trading/execution/execution_report_handler.hpp"

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

trading::Order* create_submitted_order(
    trading::OrderManager& manager,
    trading::Side side = trading::Side::Buy,
    trading::Quantity quantity = 100)
{
    trading::Order* order = manager.create_order(trading::Symbol("AAPL"), side, 10000, quantity, 1);
    const bool submitted = manager.submit(order->client_order_id);
    (void)submitted;
    return order;
}

trading::ExecutionReport report(
    trading::ClientOrderId client_order_id,
    trading::ExecType type,
    trading::OrderStatus status,
    trading::Quantity last_quantity = 0,
    trading::Price last_price = 0,
    trading::Quantity cumulative = 0,
    trading::Quantity leaves = 0,
    trading::Side side = trading::Side::Buy)
{
    trading::ExecutionReport execution_report{};
    execution_report.client_order_id = client_order_id;
    execution_report.exchange_order_id = 9001;
    execution_report.exec_type = type;
    execution_report.order_status = status;
    execution_report.symbol = trading::Symbol("AAPL");
    execution_report.side = side;
    execution_report.last_quantity = last_quantity;
    execution_report.last_price = last_price;
    execution_report.cumulative_quantity = cumulative;
    execution_report.leaves_quantity = leaves;
    execution_report.exchange_timestamp = 100;
    execution_report.receive_timestamp = 100;
    return execution_report;
}

void test_new_ack_updates_oms_state(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager);

    const auto result = handler.on_report(
        report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked));

    expect_true(result.accepted, "new ack accepted", failures);
    expect_true(order->status == trading::OrderStatus::Acked, "new ack updates state", failures);
    expect_true(order->exchange_order_id == 9001, "new ack exchange id", failures);
}

void test_new_reject_updates_oms_state(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager);

    const auto result = handler.on_report(
        report(order->client_order_id, trading::ExecType::NewReject, trading::OrderStatus::Rejected));

    expect_true(result.accepted, "new reject accepted", failures);
    expect_true(order->status == trading::OrderStatus::Rejected, "new reject updates state", failures);
}

void test_partial_fill_updates_oms_and_position(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager, trading::Side::Buy, 100);
    (void)handler.on_report(report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked));

    const auto result = handler.on_report(report(order->client_order_id,
        trading::ExecType::PartialFill,
        trading::OrderStatus::PartiallyFilled,
        40,
        10000,
        40,
        60,
        trading::Side::Buy));

    expect_true(result.accepted, "partial fill accepted", failures);
    expect_true(order->status == trading::OrderStatus::PartiallyFilled, "partial fill state", failures);
    expect_true(order->filled_quantity == 40, "partial fill cumulative", failures);
    expect_true(order->remaining_quantity == 60, "partial fill remaining", failures);
    expect_true(positions.position() == 40, "partial fill position", failures);
    expect_true(positions.total_bought() == 40, "partial fill total bought", failures);
}

void test_full_fill_updates_oms_and_position(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager, trading::Side::Sell, 50);
    (void)handler.on_report(report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked));

    const auto result = handler.on_report(report(order->client_order_id,
        trading::ExecType::Fill,
        trading::OrderStatus::Filled,
        50,
        10000,
        50,
        0,
        trading::Side::Sell));

    expect_true(result.accepted, "full fill accepted", failures);
    expect_true(order->status == trading::OrderStatus::Filled, "full fill state", failures);
    expect_true(order->filled_quantity == 50, "full fill cumulative", failures);
    expect_true(order->remaining_quantity == 0, "full fill remaining", failures);
    expect_true(positions.position() == -50, "full fill position", failures);
    expect_true(positions.total_sold() == 50, "full fill total sold", failures);
}

void test_cancel_ack_updates_oms_state(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager);
    (void)handler.on_report(report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked));
    (void)manager.request_cancel(order->client_order_id);

    const auto result = handler.on_report(
        report(order->client_order_id, trading::ExecType::CancelAck, trading::OrderStatus::Canceled));

    expect_true(result.accepted, "cancel ack accepted", failures);
    expect_true(order->status == trading::OrderStatus::Canceled, "cancel ack state", failures);
}

void test_cancel_reject_updates_oms_state(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager);
    (void)handler.on_report(report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked));
    (void)manager.request_cancel(order->client_order_id);

    const auto result = handler.on_report(
        report(order->client_order_id, trading::ExecType::CancelReject, trading::OrderStatus::Acked));

    expect_true(result.accepted, "cancel reject accepted", failures);
    expect_true(order->status == trading::OrderStatus::Acked, "cancel reject state", failures);
}

void test_duplicate_report_handling(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager);
    const auto ack = report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked);

    const auto first = handler.on_report(ack);
    const auto second = handler.on_report(ack);

    expect_true(first.accepted, "duplicate first accepted", failures);
    expect_true(!second.accepted, "duplicate second rejected", failures);
    expect_true(second.reason == trading::ExecutionHandlerRejectReason::DuplicateReport, "duplicate reason", failures);
}

void test_unknown_order_report_rejected(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);

    const auto result = handler.on_report(report(999, trading::ExecType::NewAck, trading::OrderStatus::Acked));

    expect_true(!result.accepted, "unknown report rejected", failures);
    expect_true(result.reason == trading::ExecutionHandlerRejectReason::UnknownOrder, "unknown report reason", failures);
}

void test_invalid_fill_report_rejected_without_position_update(int& failures)
{
    trading::OrderManager manager;
    trading::PositionManager positions;
    trading::ExecutionReportHandler handler(manager, positions);
    trading::Order* order = create_submitted_order(manager, trading::Side::Buy, 100);
    (void)handler.on_report(report(order->client_order_id, trading::ExecType::NewAck, trading::OrderStatus::Acked));

    const auto result = handler.on_report(report(order->client_order_id,
        trading::ExecType::PartialFill,
        trading::OrderStatus::PartiallyFilled,
        0,
        10000,
        0,
        100,
        trading::Side::Buy));

    expect_true(!result.accepted, "invalid fill report rejected", failures);
    expect_true(result.reason == trading::ExecutionHandlerRejectReason::InvalidTransition,
        "invalid fill report reason", failures);
    expect_true(order->status == trading::OrderStatus::Acked, "invalid fill report preserves order state", failures);
    expect_true(positions.position() == 0, "invalid fill report does not update position", failures);
}

} // namespace

int run_execution_report_handler_tests()
{
    int failures = 0;

    test_new_ack_updates_oms_state(failures);
    test_new_reject_updates_oms_state(failures);
    test_partial_fill_updates_oms_and_position(failures);
    test_full_fill_updates_oms_and_position(failures);
    test_cancel_ack_updates_oms_state(failures);
    test_cancel_reject_updates_oms_state(failures);
    test_duplicate_report_handling(failures);
    test_unknown_order_report_rejected(failures);
    test_invalid_fill_report_rejected_without_position_update(failures);

    return failures;
}
