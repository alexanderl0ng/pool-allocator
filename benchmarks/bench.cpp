#include "../pool_allocator.hpp"

#include <benchmark/benchmark.h>
#include <vector>

struct Widget {
    double a;
    int b;
    int c;
};

static void BM_NewDelete(benchmark::State& state) {
    const std::size_t num_elements = state.range(0);

    std::vector<Widget*> ptrs(num_elements);
    for (auto _ : state) {
        for (std::size_t i{}; i < num_elements; ++i) {
            ptrs[i] = new Widget(1.0, 2, 3);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i{}; i < num_elements; ++i) {
            delete ptrs[i];
        }
    }
}

template <std::size_t Capacity>
static void BM_PoolAllocator(benchmark::State& state) {
    const std::size_t num_elements = state.range(0);

    pool::PoolAllocator<Widget, Capacity> p;
    std::vector<Widget*> ptrs(num_elements);
    for (auto _ : state) {
        for (std::size_t i{}; i < num_elements; ++i) {
            ptrs[i] = p.create(1.0, 2, 3);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i{}; i < num_elements; ++i) {
            p.deallocate(ptrs[i]);
        }
    }
}

BENCHMARK(BM_NewDelete)
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(10'000);

BENCHMARK_TEMPLATE(BM_PoolAllocator, 10'000)
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(10'000);

BENCHMARK_MAIN();
