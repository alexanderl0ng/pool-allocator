#pragma once
#include <cstddef>
#include <cassert>
#include <utility>
#include <limits>
#include <type_traits>
#include <new>

namespace pool {

template<typename T, std::size_t N>
class PoolAllocator {

public:
    PoolAllocator() {
        for (std::size_t i = 0; i < N - 1; ++i) {
            void* slot = buffer_ + i * slot_size; 
            void* next = buffer_ + (i + 1) * slot_size;
            *reinterpret_cast<void**>(slot) = next;
        }

        *reinterpret_cast<void**>(buffer_ + (N - 1) * slot_size) = nullptr;
        free_head_ = buffer_;
    }
    
    PoolAllocator(const PoolAllocator& other) = delete;
    PoolAllocator(PoolAllocator&& other) = delete;

    PoolAllocator& operator=(const PoolAllocator& other) = delete;
    PoolAllocator& operator=(PoolAllocator&& other) = delete;

    ~PoolAllocator() {
    #ifdef DEBUG
        assert(count_free_() == N && "[ERROR] PoolAllocator destroyed with live objects still allocated!");
    #endif
    }

    template<typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        T* obj_ptr = allocate();
        if (!obj_ptr) return nullptr;
        return ::new(obj_ptr) T(std::forward<Args>(args)...);
    }

    [[nodiscard]] T* allocate() noexcept {
        if (!free_head_) return nullptr;
        T* obj_ptr = reinterpret_cast<T*>(free_head_);
        free_head_ = *reinterpret_cast<void**>(free_head_);
        return obj_ptr;
    }

    // Precondition: obj_ptr must have been returned by a prior call to allocate() or create()
    // on this same pool, and must not have already been deallocated
    void deallocate(T* obj_ptr) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            obj_ptr->~T();
        }
        *reinterpret_cast<void**>(obj_ptr) = free_head_;
        free_head_ = obj_ptr;
    }

private:
    static constexpr std::size_t slot_size = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);

    static_assert(N > 0, "PoolAllocator requires N > 0");
    static_assert(N <= std::numeric_limits<std::size_t>::max() / slot_size,
                  "PoolAllocator<T, N>: N * slot_size overflows std::size_t");

    alignas(alignof(T) > alignof(void*) ? alignof(T) : alignof(void*)) std::byte buffer_[N * slot_size];
    void* free_head_;

    #ifdef DEBUG
    std::size_t count_free_() const noexcept {
        std::size_t count {};
        void* node = free_head_;
        while (node) {
            ++count;
            node = *reinterpret_cast<void**>(node);
        }
        return count;
    }
    #endif
};

}
