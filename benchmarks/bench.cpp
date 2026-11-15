#include "../pool_allocator.hpp"
#include "order.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <new>
#include <random>
#include <vector>

#ifndef NDEBUG
#warning "NDEBUG is not defined: PoolAllocator debug checks are enabled and results will be skewed. Build with -DNDEBUG."
#endif

using lob::Order;

namespace {

constexpr std::size_t kPoolCapacity  = 1 << 17;
constexpr std::size_t kNumOps        = 100'000;
constexpr std::size_t kWarmupPasses  = 3;
constexpr std::size_t kLatencyPasses = 10;

struct Op {
    bool          is_create;
    std::uint32_t idx;
};

std::vector<Op> make_script(std::size_t resting) {
    std::mt19937 rng(42);
    std::bernoulli_distribution coin(0.5);

    std::vector<Op> ops;
    std::size_t live = resting;

    auto push_create = [&] {
        ops.push_back({true, 0});
        ++live;
    };
    auto push_delete = [&] {
        std::uniform_int_distribution<std::size_t> pick(0, live - 1);
        ops.push_back({false, static_cast<std::uint32_t>(pick(rng))});
        --live;
    };

    for (std::size_t i = 0; i < kNumOps; ++i) {
        bool create = coin(rng);
        if (live == 0)             create = true;
        if (live == kPoolCapacity) create = false;
        create ? push_create() : push_delete();
    }
    while (live > resting) push_delete();
    while (live < resting) push_create();
    return ops;
}

void fill_order(Order* o, std::uint64_t id) {
    o->id_number     = id;
    o->limit         = 10'000 + (id % 64);
    o->entry_time    = id;
    o->event_time    = id;
    o->shares        = 100;
    o->side          = (id % 2) ? lob::Side::Buy : lob::Side::Sell;
    o->type          = lob::OrderType::Limit;
    o->time_in_force = lob::TimeInForce::GoodTillCancel;
}

void remove_at(std::vector<Order*>& live, std::size_t idx) {
    live[idx] = live.back();
    live.pop_back();
}

struct Baseline {
    Order scratch;

    Order* create()         { return ::new (&scratch) Order{}; }
    void   destroy(Order*)  {}
};

struct NewDelete {
    Order* create()          { return new Order{}; }
    void   destroy(Order* o) { delete o; }
};

struct Pmr {
    std::pmr::unsynchronized_pool_resource pool;

    Order* create() {
        void* mem = pool.allocate(sizeof(Order), alignof(Order));
        return ::new (mem) Order{};
    }
    void destroy(Order* o) {
        std::destroy_at(o);
        pool.deallocate(o, sizeof(Order), alignof(Order));
    }
};

struct Pool {
    std::unique_ptr<pool::PoolAllocator<Order, kPoolCapacity>> pool =
        std::make_unique<pool::PoolAllocator<Order, kPoolCapacity>>();

    Order* create()          { return pool->create(); }
    void   destroy(Order* o) { pool->destroy(o); }
};

template <typename Alloc>
inline void apply(Alloc& alloc, const Op& op, std::vector<Order*>& live, std::uint64_t& id) {
    if (op.is_create) {
        Order* o = alloc.create();
        fill_order(o, id++);
        benchmark::DoNotOptimize(o);
        live.push_back(o);
    } else {
        alloc.destroy(live[op.idx]);
        remove_at(live, op.idx);
    }
}

void report_time_per_op(benchmark::State& state, std::size_t ops) {
    state.counters["time/op"] = benchmark::Counter(
        static_cast<double>(ops),
        benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

// `samples` holds one duration in seconds per op.
void report_percentiles(benchmark::State& state, std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());
    auto at = [&](double q) {
        const auto i = static_cast<std::size_t>(q * static_cast<double>(samples.size()));
        return samples[std::min(i, samples.size() - 1)];
    };
    state.counters["p50"]   = at(0.50);
    state.counters["p99"]   = at(0.99);
    state.counters["p99.9"] = at(0.999);
    state.counters["max"]   = samples.back();
}

template <typename Alloc>
void BM_Churn(benchmark::State& state) {
    const auto resting = static_cast<std::size_t>(state.range(0));
    if (resting > kPoolCapacity) {
        state.SkipWithError("resting exceeds kPoolCapacity");
        return;
    }

    const std::vector<Op> script = make_script(resting);
    Alloc alloc;
    std::vector<Order*> live;
    live.reserve(kPoolCapacity);
    std::uint64_t id = 0;

    for (std::size_t i = 0; i < resting; ++i) {
        live.push_back(alloc.create());
        fill_order(live.back(), id++);
    }

    for (std::size_t pass = 0; pass < kWarmupPasses; ++pass)
        for (const Op& op : script) apply(alloc, op, live, id);

    for (auto _ : state)
        for (const Op& op : script) apply(alloc, op, live, id);

    using Clock = std::chrono::steady_clock;
    std::vector<double> samples;
    samples.reserve(kLatencyPasses * script.size());
    for (std::size_t pass = 0; pass < kLatencyPasses; ++pass) {
        for (const Op& op : script) {
            const auto t0 = Clock::now();
            apply(alloc, op, live, id);
            const auto t1 = Clock::now();
            samples.push_back(std::chrono::duration<double>(t1 - t0).count());
        }
    }

    for (Order* o : live) alloc.destroy(o);
    report_time_per_op(state, script.size());
    report_percentiles(state, samples);
}

}  // namespace

BENCHMARK_TEMPLATE(BM_Churn, Baseline) ->Arg(1'000)->Arg(10'000)->Arg(100'000);
BENCHMARK_TEMPLATE(BM_Churn, NewDelete)->Arg(1'000)->Arg(10'000)->Arg(100'000);
BENCHMARK_TEMPLATE(BM_Churn, Pmr)      ->Arg(1'000)->Arg(10'000)->Arg(100'000);
BENCHMARK_TEMPLATE(BM_Churn, Pool)     ->Arg(1'000)->Arg(10'000)->Arg(100'000);

BENCHMARK_MAIN();