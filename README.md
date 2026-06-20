# pool-allocator

A single-header pool allocator in C++.

## Usage

### Basic Allocation

```cpp
#include "pool_allocator.hpp"

pool::PoolAllocator<double, 10> p;

// Allocate the raw memory for trivial types
double* d = p.allocate();

// Assigning a value to the allocated memory
*d = 3.14;

// Must deallocate the memory
p.deallocate(d);
```

### Non-Trivial Types
Use `create()` to construct non-trivially destructible types in the arena. Destructors are automatically called on `deallocate()`:
```cpp
struct TestStruct {
    double d;
    int i;
    long l;
};

pool::PoolAllocator<TestStruct, 10> p;

TestStruct* a = p.create(2.0, 1, 1'000'000'000);

// ~TestStruct is called here
p.deallocate(a);
```

### Moving the Pool
The pool cannot be moved, it cannot be copied. Attempting to move or copy the pool will cause the compiler to error.

### Memory Safety
`deallocate()` must only be called once per object, and only on a pointer previously returned by `allocate()/create()` from the same pool. There are no runtime checks for double freeing or freeing foreign pointers.

All objects must be deallocated before the pool itself is destroyed if `T` has a non-trivial destructor. In debug build the pool destructor will assert if any object remains allocated within the pool.

## Benchmarks
```
-------------------------------------------------------------------------
Benchmark                               Time             CPU   Iterations
-------------------------------------------------------------------------
BM_NewDelete/100                      607 ns          607 ns      1126036
BM_NewDelete/1000                    5806 ns         5794 ns       118682
BM_NewDelete/10000                  59208 ns        58811 ns        12164
BM_PoolAllocator<10'000>/100         50.7 ns         50.7 ns     13784140
BM_PoolAllocator<10'000>/1000         462 ns          461 ns      1572115
BM_PoolAllocator<10'000>/10000       4536 ns         4534 ns       154413
```

## Design
- **Stack Allocation** - the entire pool lives inline as a fixed-size array, there is no heap allocation. Capacity is a compile time template parameter.
- **Intrusive Freelist** - free slots store a pointer to the next free slot directly inside their own memory, removing the need for a separate array. This means `create()/allocate()/deallocate()` are O(1) operations.
- **No Liveness Tracking** - in a bid to save memory, the pool does not keep track of which slots are occupied versus available. This keeps the hotpath branch free, but it requires the caller's responsibility to `deallocate()` every object. In debug an assert checks that all objects are deallocated.
- **Alignment Safe** - `slot_size` and alignment are computed to be the larger between `sizeof(object)` and `sizeof(void*)`. This ensures there is always capacity for the pointer to next free slot.

## Building
```bash
make debug      # builds debug/bench and debug/tests
make release    # builds release/bench and release/tests
make run-debug-tests
make run-release
```
