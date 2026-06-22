#pragma once

#include <array>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace trading {

template <typename T, std::size_t Capacity>
class ObjectPool {
public:
    static_assert(Capacity > 0, "Capacity must be positive");

    ObjectPool() noexcept
    {
        reset_free_list();
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ~ObjectPool() noexcept
    {
        destroy_live_objects();
    }

    // Object pools reduce latency jitter by replacing general-purpose heap
    // allocation with fixed-cost slot reuse. Trading hot paths avoid malloc/free
    // because allocator locks, metadata updates, page faults, and fragmentation
    // can add unpredictable tail latency. The pool owns object lifetime: allocate
    // constructs in-place, and deallocate must be called exactly once for each
    // live object returned by this pool.
    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
    {
        if (free_head_ == npos) {
            return nullptr;
        }

        const std::size_t index = free_head_;
        T* object = new (&slots_[index].storage) T(std::forward<Args>(args)...);
        free_head_ = slots_[index].next_free;
        slots_[index].occupied = true;
        ++used_;
        return object;
    }

    void deallocate(T* object) noexcept
    {
        const std::size_t index = index_of(object);
        if (index == npos || !slots_[index].occupied) {
            // Invalid deallocation is ignored deliberately. This keeps the pool
            // safe for tests and demos, but production code should generally
            // treat invalid ownership as a serious programming error or assert
            // in debug builds.
            return;
        }

        object->~T();
        slots_[index].occupied = false;
        slots_[index].next_free = free_head_;
        free_head_ = index;
        --used_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return Capacity;
    }

    [[nodiscard]] std::size_t used() const noexcept
    {
        return used_;
    }

    [[nodiscard]] std::size_t available() const noexcept
    {
        return Capacity - used_;
    }

private:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    struct Slot {
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
        std::size_t next_free{npos};
        bool occupied{false};
    };

    void reset_free_list() noexcept
    {
        for (std::size_t i = 0; i < Capacity; ++i) {
            slots_[i].next_free = i + 1;
        }
        slots_[Capacity - 1].next_free = npos;
        free_head_ = 0;
    }

    void destroy_live_objects() noexcept
    {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (slots_[i].occupied) {
                ptr_at(i)->~T();
                slots_[i].occupied = false;
            }
        }
    }

    [[nodiscard]] T* ptr_at(std::size_t index) noexcept
    {
        return reinterpret_cast<T*>(&slots_[index].storage);
    }

    [[nodiscard]] const T* ptr_at(std::size_t index) const noexcept
    {
        return reinterpret_cast<const T*>(&slots_[index].storage);
    }

    [[nodiscard]] std::size_t index_of(const T* object) const noexcept
    {
        if (object == nullptr) {
            return npos;
        }

        for (std::size_t i = 0; i < Capacity; ++i) {
            if (ptr_at(i) == object) {
                return i;
            }
        }
        return npos;
    }

    // This simple pool is single-threaded or externally synchronized. It does
    // not solve ABA hazards, cross-thread ownership, object lifetime auditing,
    // or lock-free reclamation.
    std::array<Slot, Capacity> slots_{};
    std::size_t free_head_{npos};
    std::size_t used_{0};
};

} // namespace trading
