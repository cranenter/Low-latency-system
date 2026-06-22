#include "trading/oms/order_state_machine.hpp"

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

trading::Order make_order(trading::Quantity quantity = 100)
{
    return trading::Order(
        1,
        trading::Symbol("AAPL"),
        trading::Side::Buy,
        trading::OrderType::Limit,
        trading::TimeInForce::Day,
        10000,
        quantity,
        100);
}

void apply_ignored(
    trading::Order& order,
    trading::OrderEvent event,
    trading::Quantity fill_quantity = 0,
    trading::Price fill_price = 0)
{
    const auto transition = trading::OrderStateMachine::apply(order, event, fill_quantity, fill_price);
    (void)transition;
}

void test_created_pending_new_acked(int& failures)
{
    auto order = make_order();

    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::Submit).accepted,
        "submit accepted", failures);
    expect_true(order.status == trading::OrderStatus::PendingNew, "submit state", failures);
    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::NewAck).accepted,
        "ack accepted", failures);
    expect_true(order.status == trading::OrderStatus::Acked, "ack state", failures);
}

void test_pending_new_rejected(int& failures)
{
    auto order = make_order();

    apply_ignored(order, trading::OrderEvent::Submit);
    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::NewReject).accepted,
        "new reject accepted", failures);
    expect_true(order.status == trading::OrderStatus::Rejected, "new reject state", failures);
}

void test_acked_partially_filled_filled(int& failures)
{
    auto order = make_order(100);
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);

    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::PartialFill, 40, 10000).accepted,
        "partial fill accepted", failures);
    expect_true(order.status == trading::OrderStatus::PartiallyFilled, "partial fill state", failures);
    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::Fill, 60, 10010).accepted,
        "fill accepted", failures);
    expect_true(order.status == trading::OrderStatus::Filled, "filled state", failures);
}

void test_cancel_ack_path(int& failures)
{
    auto order = make_order();
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);

    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::CancelRequest).accepted,
        "cancel request accepted", failures);
    expect_true(order.status == trading::OrderStatus::PendingCancel, "pending cancel state", failures);
    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::CancelAck).accepted,
        "cancel ack accepted", failures);
    expect_true(order.status == trading::OrderStatus::Canceled, "canceled state", failures);
}

void test_cancel_reject_returns_to_active_state(int& failures)
{
    auto order = make_order();
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);
    apply_ignored(order, trading::OrderEvent::CancelRequest);

    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::CancelReject).accepted,
        "cancel reject accepted", failures);
    expect_true(order.status == trading::OrderStatus::Acked, "cancel reject returns acked", failures);
}

void test_partial_fill_then_cancel(int& failures)
{
    auto order = make_order(100);
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);
    apply_ignored(order, trading::OrderEvent::PartialFill, 25, 10000);
    apply_ignored(order, trading::OrderEvent::CancelRequest);

    expect_true(order.status == trading::OrderStatus::PendingCancel, "partial cancel pending", failures);
    expect_true(order.filled_quantity == 25, "partial cancel filled quantity", failures);
    expect_true(order.remaining_quantity == 75, "partial cancel remaining quantity", failures);
    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::CancelAck).accepted,
        "partial cancel ack", failures);
    expect_true(order.status == trading::OrderStatus::Canceled, "partial canceled state", failures);
}

void test_fill_while_cancel_pending(int& failures)
{
    auto order = make_order(100);
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);
    apply_ignored(order, trading::OrderEvent::CancelRequest);

    expect_true(trading::OrderStateMachine::apply(order, trading::OrderEvent::Fill, 100, 10000).accepted,
        "fill while cancel pending accepted", failures);
    expect_true(order.status == trading::OrderStatus::Filled, "fill while cancel pending state", failures);
    expect_true(order.remaining_quantity == 0, "fill while cancel pending remaining", failures);
}

void test_invalid_transitions_rejected(int& failures)
{
    auto order = make_order();

    const auto invalid_ack = trading::OrderStateMachine::apply(order, trading::OrderEvent::NewAck);
    expect_true(!invalid_ack.accepted, "ack before submit rejected", failures);
    expect_true(order.status == trading::OrderStatus::Created, "invalid transition preserves state", failures);

    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewReject);
    const auto invalid_cancel = trading::OrderStateMachine::apply(order, trading::OrderEvent::CancelRequest);
    expect_true(!invalid_cancel.accepted, "cancel rejected order rejected", failures);
}

void test_invalid_fill_details_are_rejected(int& failures)
{
    auto order = make_order(100);
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);

    const auto zero_quantity =
        trading::OrderStateMachine::apply(order, trading::OrderEvent::PartialFill, 0, 10000);
    const auto zero_price =
        trading::OrderStateMachine::apply(order, trading::OrderEvent::Fill, 10, 0);

    expect_true(!zero_quantity.accepted, "zero quantity fill rejected", failures);
    expect_true(!zero_price.accepted, "zero price fill rejected", failures);
    expect_true(order.status == trading::OrderStatus::Acked, "invalid fill preserves state", failures);
    expect_true(order.filled_quantity == 0, "invalid fill preserves filled quantity", failures);
    expect_true(order.remaining_quantity == 100, "invalid fill preserves remaining quantity", failures);
}

void test_fill_accounting(int& failures)
{
    auto order = make_order(100);
    apply_ignored(order, trading::OrderEvent::Submit);
    apply_ignored(order, trading::OrderEvent::NewAck);
    apply_ignored(order, trading::OrderEvent::PartialFill, 25, 10000);
    apply_ignored(order, trading::OrderEvent::PartialFill, 25, 10020);

    expect_true(order.filled_quantity == 50, "cumulative fill quantity", failures);
    expect_true(order.remaining_quantity == 50, "remaining quantity", failures);
    expect_true(order.average_fill_price == 10010, "average fill price", failures);
}

} // namespace

int run_order_state_machine_tests()
{
    int failures = 0;

    test_created_pending_new_acked(failures);
    test_pending_new_rejected(failures);
    test_acked_partially_filled_filled(failures);
    test_cancel_ack_path(failures);
    test_cancel_reject_returns_to_active_state(failures);
    test_partial_fill_then_cancel(failures);
    test_fill_while_cancel_pending(failures);
    test_invalid_transitions_rejected(failures);
    test_invalid_fill_details_are_rejected(failures);
    test_fill_accounting(failures);

    return failures;
}
