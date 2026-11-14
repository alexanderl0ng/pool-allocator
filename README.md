# pool-allocator

A single-header, fixed-capacity pool allocator in C++. It hands out slots for one type `T` in O(1), with no heap allocation and no system calls after construction.

Requires C++20 or later. The benchmarks also need [Google Benchmark](https://github.com/google/benchmark).

## Usage

### Basic Allocation

```cpp
#include "pool_allocator.hpp"

pool::PoolAllocator<double, 10> p;

// Get uninitialised storage for a trivial type
double* d = p.allocate();
if (!d) { /* pool is full */ }

// Assign a value to the allocated memory
*d = 3.14;

// Return the slot to the pool
p.destroy(d);
```

`allocate()` is only available for trivially constructible and trivially destructible types. The memory it returns is uninitialised.

### Non-Trivial Types
Use `create()` to construct an object in the pool, forwarding any constructor arguments. The destructor is called automatically on `destroy()`:
```cpp
struct Trade {
    std::string symbol;
    double      price;
};

pool::PoolAllocator<Trade, 10> p;

Trade* t = p.create("AAPL", 189.5);

// ~Trade is called here, then the slot is returned to the pool
p.destroy(t);
```

If the constructor throws, the slot is returned to the pool and the exception propagates.

### When the Pool Is Full
`create()` and `allocate()` return `nullptr` when all `N` slots are in use. The pool never grows.

`size()` returns the number of slots currently in use and `capacity()` returns `N`.

### Copying and Moving
The pool cannot be moved and it cannot be copied. Free slots point into the pool's own storage, and any pointers already handed out would dangle. Attempting to move or copy the pool is a compile error.

### Memory Safety
`destroy()` must only be called once per object, and only on a pointer previously returned by `allocate()`/`create()` from the same pool. Passing `nullptr` is safe and does nothing. In release builds there are no runtime checks for double freeing or freeing foreign pointers. In debug builds both trigger an assert.

All objects should be destroyed before the pool itself is destroyed. In debug builds the pool destructor asserts if any object is still live. In release builds any remaining objects do not have their destructors run.

### Multithreading
The pool is not thread safe. Each thread should have its own pool, and an object must be destroyed on the same thread, through the same pool, that created it.

## Benchmarks
```
-------------------------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations UserCounters...
-------------------------------------------------------------------------------------
BM_Churn<Baseline>/1000        271133 ns       270796 ns         2547 time/op=2.70721ns
BM_Churn<Baseline>/10000       271551 ns       271230 ns         2540 time/op=2.71154ns
BM_Churn<Baseline>/100000      271142 ns       270521 ns         2551 time/op=2.70451ns
BM_Churn<NewDelete>/1000       774766 ns       773853 ns          915 time/op=7.73636ns
BM_Churn<NewDelete>/10000      846782 ns       842613 ns          834 time/op=8.42377ns
BM_Churn<NewDelete>/100000     928885 ns       927534 ns          752 time/op=9.27293ns
BM_Churn<Pmr>/1000            1566103 ns      1563364 ns          447 time/op=15.6293ns
BM_Churn<Pmr>/10000           1987678 ns      1985322 ns          353 time/op=19.8477ns
BM_Churn<Pmr>/100000          2938173 ns      2933749 ns          236 time/op=29.3299ns
BM_Churn<Pool>/1000            337292 ns       333861 ns         2088 time/op=3.33768ns
BM_Churn<Pool>/10000           337989 ns       337483 ns         2076 time/op=3.37389ns
BM_Churn<Pool>/100000          368792 ns       367954 ns         1886 time/op=3.67858ns
```
Benchmarks run on an AMD Ryzen 7 9800X3D under Arch Linux, built with `g++ -std=c++23 -O2 -DNDEBUG`. See `benchmarks/bench.cpp` for details.

How to read this:
- **Workload** - a fixed random script of roughly 100,000 creates and deletes of a 64-byte, cache-line-aligned `Order`. The argument (`/1000`, `/10000`, `/100000`) is the number of orders resting in the pool while the script runs.
- **`time/op`** - the mean time for one create or delete.
- **`Baseline`** - the same loop with no allocator, so it measures the benchmark's own overhead. Subtract it from the other rows to get the cost of the allocator alone.

Allocator cost per operation, after subtracting the baseline:

| Resting orders | Pool    | `new`/`delete` | `std::pmr` pool |
|----------------|---------|----------------|-----------------|
| 1,000          | 0.63 ns | 5.03 ns        | 12.9 ns         |
| 10,000         | 0.66 ns | 5.71 ns        | 17.1 ns         |
| 100,000        | 0.97 ns | 6.57 ns        | 26.6 ns         |

## Design
- **Inline Storage** - the entire pool lives inline as a fixed-size array and never allocates. Capacity is a compile time template parameter. Where the pool lives is up to the caller: small pools can sit on the stack, but larger pools should be placed on the heap with `make_unique`. For example, 131,072 slots of a 64-byte type is 8 MB, about the size of a default stack.
- **Intrusive Freelist** - free slots store a pointer to the next free slot directly inside their own memory, removing the need for a separate array. This means `create()`/`allocate()`/`destroy()` are O(1) operations.
- **No Per-Slot Tracking in Release** - to save memory and keep the hot path short, release builds do not record which slots are occupied, only how many. It is the caller's responsibility to `destroy()` every object exactly once. In debug builds the pool tracks every slot and asserts that pointers not handed out by the pool cannot be `destroy()`ed, that no slot is freed twice, and that no live objects remain upon pool destruction.
- **Alignment Safe** - each slot is sized and aligned to the larger of `T` and a pointer, so there is always room for the pointer to the next free slot. Over-aligned types such as `alignas(64)` are supported.

## Building
```bash
make debug      # builds debug/bench and debug/test
make release    # builds release/bench and release/test
make run-debug-tests
make run-release
```