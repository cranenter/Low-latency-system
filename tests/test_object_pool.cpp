#include "trading/utils/object_pool.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect_true(bool condition, const std::string& name, int& failures)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << name << '\n';
    }
}

struct TestObject {
    static int constructions;
    static int destructions;

    int id{};
    int value{};

    TestObject(int object_id, int object_value) noexcept
        : id(object_id)
        , value(object_value)
    {
        ++constructions;
    }

    ~TestObject() noexcept
    {
        ++destructions;
    }
};

int TestObject::constructions = 0;
int TestObject::destructions = 0;

void reset_counts()
{
    TestObject::constructions = 0;
    TestObject::destructions = 0;
}

void test_allocate_one_object(int& failures)
{
    reset_counts();
    trading::ObjectPool<TestObject, 2> pool;

    TestObject* object = pool.allocate(1, 100);

    expect_true(object != nullptr, "allocate one returns object", failures);
    expect_true(pool.used() == 1, "allocate one used count", failures);
    expect_true(pool.available() == 1, "allocate one available count", failures);
    expect_true(TestObject::constructions == 1, "allocate one construction count", failures);

    pool.deallocate(object);
}

void test_allocate_many_objects(int& failures)
{
    reset_counts();
    trading::ObjectPool<TestObject, 3> pool;

    TestObject* first = pool.allocate(1, 10);
    TestObject* second = pool.allocate(2, 20);
    TestObject* third = pool.allocate(3, 30);

    expect_true(first != nullptr, "allocate many first", failures);
    expect_true(second != nullptr, "allocate many second", failures);
    expect_true(third != nullptr, "allocate many third", failures);
    expect_true(pool.used() == 3, "allocate many used count", failures);
    expect_true(first != second && second != third && first != third, "allocate many unique slots", failures);

    pool.deallocate(first);
    pool.deallocate(second);
    pool.deallocate(third);
}

void test_pool_exhaustion(int& failures)
{
    reset_counts();
    trading::ObjectPool<TestObject, 1> pool;

    TestObject* object = pool.allocate(1, 10);
    TestObject* exhausted = pool.allocate(2, 20);

    expect_true(object != nullptr, "exhaustion first allocation", failures);
    expect_true(exhausted == nullptr, "exhaustion returns null", failures);
    expect_true(pool.used() == 1, "exhaustion used count", failures);
    expect_true(TestObject::constructions == 1, "exhaustion does not construct extra object", failures);

    pool.deallocate(object);
}

void test_deallocate_and_reuse(int& failures)
{
    reset_counts();
    trading::ObjectPool<TestObject, 1> pool;

    TestObject* first = pool.allocate(1, 10);
    pool.deallocate(first);
    TestObject* second = pool.allocate(2, 20);

    expect_true(second == first, "reuse same slot", failures);
    expect_true(second != nullptr && second->id == 2, "reuse reconstructed id", failures);
    expect_true(second != nullptr && second->value == 20, "reuse reconstructed value", failures);
    expect_true(TestObject::constructions == 2, "reuse construction count", failures);
    expect_true(TestObject::destructions == 1, "reuse destruction count before final deallocate", failures);

    pool.deallocate(second);
}

void test_destructor_called(int& failures)
{
    reset_counts();

    {
        trading::ObjectPool<TestObject, 2> pool;
        TestObject* first = pool.allocate(1, 10);
        TestObject* second = pool.allocate(2, 20);
        pool.deallocate(first);

        expect_true(TestObject::destructions == 1, "explicit deallocate destructor", failures);
        expect_true(second != nullptr, "second allocation exists", failures);
    }

    expect_true(TestObject::constructions == 2, "destructor test construction count", failures);
    expect_true(TestObject::destructions == 2, "pool destructor destroys remaining live object", failures);
}

void test_object_fields_after_construction(int& failures)
{
    reset_counts();
    trading::ObjectPool<TestObject, 1> pool;

    TestObject* object = pool.allocate(42, 9001);

    expect_true(object != nullptr, "fields allocation", failures);
    expect_true(object != nullptr && object->id == 42, "constructed id field", failures);
    expect_true(object != nullptr && object->value == 9001, "constructed value field", failures);

    pool.deallocate(object);
}

void test_invalid_deallocation_is_ignored(int& failures)
{
    reset_counts();
    trading::ObjectPool<TestObject, 1> pool;
    TestObject external(9, 99);

    pool.deallocate(nullptr);
    pool.deallocate(&external);

    expect_true(pool.used() == 0, "invalid deallocation leaves used count", failures);
    expect_true(pool.available() == 1, "invalid deallocation leaves available count", failures);
    expect_true(TestObject::destructions == 0, "invalid deallocation does not destroy external object yet", failures);
}

struct ThrowingObject {
    explicit ThrowingObject(bool should_throw)
    {
        if (should_throw) {
            throw std::runtime_error("construction failed");
        }
    }
};

void test_throwing_constructor_preserves_slot(int& failures)
{
    trading::ObjectPool<ThrowingObject, 1> pool;

    try {
        (void)pool.allocate(true);
        expect_true(false, "throwing constructor should throw", failures);
    } catch (const std::runtime_error&) {
        expect_true(pool.used() == 0, "throwing constructor keeps used count", failures);
        expect_true(pool.available() == 1, "throwing constructor keeps slot available", failures);
    }

    ThrowingObject* object = pool.allocate(false);
    expect_true(object != nullptr, "slot reusable after throwing constructor", failures);
    pool.deallocate(object);
}

} // namespace

int run_object_pool_tests()
{
    int failures = 0;

    test_allocate_one_object(failures);
    test_allocate_many_objects(failures);
    test_pool_exhaustion(failures);
    test_deallocate_and_reuse(failures);
    test_destructor_called(failures);
    test_object_fields_after_construction(failures);
    test_invalid_deallocation_is_ignored(failures);
    test_throwing_constructor_preserves_slot(failures);

    return failures;
}
