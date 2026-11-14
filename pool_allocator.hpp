#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <utility>
#include <limits>
#include <array>
#include <type_traits>
#include <new>

#include <iostream>

namespace pool {

template<typename T, std::size_t N>
class PoolAllocator {
    union Slot {
        Slot* next;
        alignas(T) std::byte storage[sizeof(T)];
    };
    static_assert(N > 0, "PoolAllocator requires N > 0");
    static_assert(N <= std::numeric_limits<std::size_t>::max() / sizeof(Slot),
                  "PoolAllocator<T, N>: N * slot_size overflows std::size_t");

    struct SlotGuard {
        PoolAllocator& owner_;
        T*             slot_;

        SlotGuard(PoolAllocator& owner_, T* slot_) noexcept : owner_(owner_), slot_(slot_) {}

        ~SlotGuard() {
            if (slot_ != nullptr) owner_.push(slot_);
        }

        void dismiss() noexcept { slot_ = nullptr; }

        SlotGuard(const SlotGuard& other) = delete;
        SlotGuard& operator=(const SlotGuard& other) = delete;
    };

public:
    PoolAllocator() {
        for (std::size_t i = 0; i < N - 1; ++i) slots_[i].next = &slots_[i + 1];

        slots_[N - 1].next = nullptr;
        free_head_ = &slots_[0];
    }
    
    PoolAllocator(const PoolAllocator& other) = delete;
    PoolAllocator(PoolAllocator&& other) = delete;

    PoolAllocator& operator=(const PoolAllocator& other) = delete;
    PoolAllocator& operator=(PoolAllocator&& other) = delete;

    ~PoolAllocator() {
#ifndef NDEBUG
        assert(live_count_ == 0 && "[ERROR] PoolAllocator destroyed with live objects still allocated");
#endif
    }

    [[nodiscard]] std::size_t size() const noexcept { return live_count_; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return N; }

    template<typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        T* ptr = pop();
        if (!ptr) return nullptr;

        SlotGuard guard{*this, ptr};
        T* obj = ::new(ptr) T(std::forward<Args>(args)...);
        guard.dismiss();
        return obj;
    }

    [[nodiscard]] T* allocate() noexcept 
        requires std::is_trivially_default_constructible_v<T>
              && std::is_trivially_destructible_v<T>
    {
        T* ptr = pop();
        return ptr ? ::new (ptr) T : nullptr;
    }

    // Precondition: obj_ptr must have been returned by a prior call to allocate() or create()
    // on this same pool, and must not have already been deallocated
    void destroy(T* obj_ptr) noexcept {
        if (!obj_ptr) return;
#ifndef NDEBUG
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(obj_ptr);
        std::uintptr_t start = reinterpret_cast<std::uintptr_t>(slots_);
        std::uintptr_t diff = p - start;
        assert(diff < N * sizeof(Slot));
        assert(diff % sizeof(Slot) == 0);
        std::size_t idx = diff / sizeof(Slot);
        assert(live_flags_[idx]);
#endif
        if constexpr (!std::is_trivially_destructible_v<T>) obj_ptr->~T();
        push(obj_ptr);
    }

private:
    [[nodiscard]] T* pop() noexcept {
        if (!free_head_) [[unlikely]] return nullptr;
        Slot* s = free_head_;
#ifndef NDEBUG
        std::ptrdiff_t idx = s - slots_;
        live_flags_[static_cast<std::size_t>(idx)] = true;
#endif
        free_head_ = s->next;
        ++live_count_;
        return reinterpret_cast<T*>(s->storage);
    }

    void push(T* p) noexcept {
        Slot* s = reinterpret_cast<Slot*>(p);
#ifndef NDEBUG
        std::ptrdiff_t idx = s - slots_;
        assert(live_flags_[static_cast<std::size_t>(idx)]);
        live_flags_[static_cast<std::size_t>(idx)] = false;
#endif        
        s->next = free_head_;
        free_head_ = s;
        --live_count_;
    }

#ifndef NDEBUG
    bool any_live_flags_() const noexcept {
        for (bool b : live_flags_) if (b) return true;

        return false;
    }
#endif

    Slot  slots_[N];
    Slot* free_head_;
    std::size_t live_count_{};
#ifndef NDEBUG
    std::array<bool, N> live_flags_{};
#endif
};

}
