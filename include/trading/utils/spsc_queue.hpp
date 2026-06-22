#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace trading {

template <typename T, std::size_t Capacity>
class SPSCQueue {
public:
    static_assert(Capacity > 0, "Capacity must be positive");
    static_assert(std::is_move_assignable<T>::value || std::is_copy_assignable<T>::value,
        "SPSCQueue requires a movable or copyable value type");

    SPSCQueue() = default;

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Single-producer/single-consumer only: exactly one thread may call push()
    // and exactly one thread may call pop(). The producer exclusively advances
    // tail and the consumer exclusively advances head, so plain load/store
    // atomics are enough. MPSC/MPMC queues need stronger coordination such as
    // compare/exchange loops because multiple threads race to publish/consume.
    [[nodiscard]] bool push(const T& item) noexcept(std::is_nothrow_copy_assignable<T>::value)
    {
        const std::size_t tail = tail_.value.load(std::memory_order_relaxed);
        const std::size_t next_tail = next(tail);

        // The producer uses acquire when reading head so it observes the
        // consumer's release after a pop before deciding whether space exists.
        // It can use relaxed for its own tail because no other producer writes it.
        if (next_tail == head_.value.load(std::memory_order_acquire)) {
            // Bounded non-blocking behavior: a full queue returns false.
            // The caller decides whether to drop, retry later, or apply backpressure.
            return false;
        }

        buffer_[tail] = item;

        // Release publishes the written slot before the consumer can observe
        // the advanced tail with an acquire load.
        tail_.value.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool push(T&& item) noexcept(std::is_nothrow_move_assignable<T>::value)
    {
        const std::size_t tail = tail_.value.load(std::memory_order_relaxed);
        const std::size_t next_tail = next(tail);

        if (next_tail == head_.value.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[tail] = std::move(item);
        tail_.value.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(T& out) noexcept(std::is_nothrow_move_assignable<T>::value)
    {
        const std::size_t head = head_.value.load(std::memory_order_relaxed);

        // The consumer uses acquire when reading tail so it observes the
        // producer's slot write before reading from the buffer. It can use
        // relaxed for its own head because no other consumer writes it.
        if (head == tail_.value.load(std::memory_order_acquire)) {
            return false;
        }

        out = std::move(buffer_[head]);
        head_.value.store(next(head), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return head_.value.load(std::memory_order_acquire)
            == tail_.value.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool full() const noexcept
    {
        const std::size_t tail = tail_.value.load(std::memory_order_acquire);
        return next(tail) == head_.value.load(std::memory_order_acquire);
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return Capacity;
    }

    [[nodiscard]] std::size_t size_approx() const noexcept
    {
        // Approximate under concurrent producer/consumer access. It is useful
        // for diagnostics, not for correctness decisions in the hot path.
        const std::size_t head = head_.value.load(std::memory_order_acquire);
        const std::size_t tail = tail_.value.load(std::memory_order_acquire);

        if (tail >= head) {
            return tail - head;
        }
        return StorageSize - head + tail;
    }

    [[nodiscard]] bool try_push(const T& item) noexcept(noexcept(push(item)))
    {
        return push(item);
    }

    [[nodiscard]] bool try_push(T&& item) noexcept(noexcept(push(std::move(item))))
    {
        return push(std::move(item));
    }

    [[nodiscard]] bool try_pop(T& out) noexcept(noexcept(pop(out)))
    {
        return pop(out);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_approx();
    }

private:
    static constexpr std::size_t StorageSize = Capacity + 1;

    struct alignas(64) AtomicIndex {
        std::atomic<std::size_t> value{0};
    };

    [[nodiscard]] static constexpr std::size_t next(std::size_t index) noexcept
    {
        return (index + 1) % StorageSize;
    }

    std::array<T, StorageSize> buffer_{};

    // Cache-line separation reduces false sharing: producer mostly writes tail,
    // consumer mostly writes head. Keeping them on separate cache lines avoids
    // unnecessary cache invalidation traffic under concurrent load.
    AtomicIndex head_{};
    AtomicIndex tail_{};
};

} // namespace trading
