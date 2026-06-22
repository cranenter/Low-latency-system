#include "trading/utils/spsc_queue.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace {

void expect_true(bool condition, const std::string& name, int& failures)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << name << '\n';
    }
}

void test_initial_empty_state(int& failures)
{
    trading::SPSCQueue<int, 4> queue;

    expect_true(queue.empty(), "initial empty", failures);
    expect_true(!queue.full(), "initial not full", failures);
    expect_true(queue.capacity() == 4, "capacity", failures);
    expect_true(queue.size_approx() == 0, "initial size", failures);
}

void test_push_then_pop(int& failures)
{
    trading::SPSCQueue<int, 4> queue;
    int out = 0;

    expect_true(queue.push(42), "push succeeds", failures);
    expect_true(!queue.empty(), "not empty after push", failures);
    expect_true(queue.pop(out), "pop succeeds", failures);
    expect_true(out == 42, "popped value", failures);
    expect_true(queue.empty(), "empty after pop", failures);
}

void test_fifo_ordering(int& failures)
{
    trading::SPSCQueue<int, 8> queue;

    for (int i = 0; i < 5; ++i) {
        expect_true(queue.push(i), "fifo push", failures);
    }

    for (int i = 0; i < 5; ++i) {
        int out = -1;
        expect_true(queue.pop(out), "fifo pop", failures);
        expect_true(out == i, "fifo value", failures);
    }
}

void test_full_queue_behavior(int& failures)
{
    trading::SPSCQueue<int, 3> queue;

    expect_true(queue.push(1), "full push 1", failures);
    expect_true(queue.push(2), "full push 2", failures);
    expect_true(queue.push(3), "full push 3", failures);
    expect_true(queue.full(), "queue full", failures);
    expect_true(!queue.push(4), "push fails when full", failures);
    expect_true(queue.size_approx() == 3, "full size", failures);
}

void test_empty_queue_behavior(int& failures)
{
    trading::SPSCQueue<int, 2> queue;
    int out = 7;

    expect_true(!queue.pop(out), "pop fails when empty", failures);
    expect_true(out == 7, "empty pop leaves output unchanged", failures);
}

void test_wrap_around_behavior(int& failures)
{
    trading::SPSCQueue<int, 3> queue;
    int out = 0;

    expect_true(queue.push(1), "wrap push 1", failures);
    expect_true(queue.push(2), "wrap push 2", failures);
    expect_true(queue.pop(out) && out == 1, "wrap pop 1", failures);
    expect_true(queue.push(3), "wrap push 3", failures);
    expect_true(queue.push(4), "wrap push 4", failures);
    expect_true(queue.full(), "wrap full", failures);

    expect_true(queue.pop(out) && out == 2, "wrap pop 2", failures);
    expect_true(queue.pop(out) && out == 3, "wrap pop 3", failures);
    expect_true(queue.pop(out) && out == 4, "wrap pop 4", failures);
    expect_true(queue.empty(), "wrap empty", failures);
}

void test_many_push_pop_operations(int& failures)
{
    trading::SPSCQueue<int, 64> queue;

    for (int i = 0; i < 10000; ++i) {
        int out = 0;
        expect_true(queue.push(i), "many push", failures);
        expect_true(queue.pop(out), "many pop", failures);
        expect_true(out == i, "many value", failures);
    }

    expect_true(queue.empty(), "many empty", failures);
}

void test_movable_type(int& failures)
{
    trading::SPSCQueue<std::unique_ptr<int>, 2> queue;

    expect_true(queue.push(std::make_unique<int>(99)), "move-only push", failures);

    std::unique_ptr<int> out;
    expect_true(queue.pop(out), "move-only pop", failures);
    expect_true(out != nullptr, "move-only not null", failures);
    expect_true(*out == 99, "move-only value", failures);
}

} // namespace

int run_spsc_queue_tests()
{
    int failures = 0;

    test_initial_empty_state(failures);
    test_push_then_pop(failures);
    test_fifo_ordering(failures);
    test_full_queue_behavior(failures);
    test_empty_queue_behavior(failures);
    test_wrap_around_behavior(failures);
    test_many_push_pop_operations(failures);
    test_movable_type(failures);

    return failures;
}
