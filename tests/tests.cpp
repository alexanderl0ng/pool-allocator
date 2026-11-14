#include "../pool_allocator.hpp"

#include <vector>
#include <cassert>
#include <iostream>
#include <string_view>
#include <set>

static std::vector<int> g_events;
struct Tracked {
    int id;
    explicit Tracked(int id) : id(id) { g_events.push_back(+id); }
    ~Tracked()                        { g_events.push_back(-id); }
};

struct TestSection {
    std::string_view name;
    TestSection(std::string_view name) : name(name) {
        std::clog << "[ RUN      ] " << name << '\n';
    }
    ~TestSection() {
        std::clog << "[     DONE ] " << name << '\n';
    }
};


int main() {
    {
    TestSection section{"General Allocation"};
    pool::PoolAllocator<Tracked, 2> p;
    auto* a = p.create(1);
    auto* b = p.create(2);
    assert(a != nullptr && b != nullptr);
    auto* c = p.create(3);
    assert(c == nullptr);
    p.destroy(a);
    p.destroy(b);
    pool::PoolAllocator<int, 1> p_2;
    int* x = p_2.allocate();
    assert(x != nullptr);
    assert(p_2.allocate() == nullptr);
    *x = 7;
    assert(*x == 7);
    p_2.destroy(x);
    }

    {
    TestSection section{"Single Slot pool"};
    pool::PoolAllocator<Tracked, 1> p;
    auto *a = p.create(1);
    assert(a != nullptr);
    assert(p.create(2) == nullptr);
    p.destroy(a);
    auto* b = p.create(3);
    assert(b != nullptr);
    p.destroy(b);
    }

    {
    TestSection section{"Alignment for Small Type"};
    struct Tiny { char c; };
    pool::PoolAllocator<Tiny, 4> p;
    auto* a = p.create('a');
    auto addr = reinterpret_cast<std::uintptr_t>(a);
    assert(addr % alignof(void*) == 0);
    p.destroy(a);
    }

    {
    TestSection section{"Freed Slot Reused"};
    pool::PoolAllocator<Tracked, 3> p;
    auto* a = p.create(1);
    auto* b = p.create(2);
    auto* c = p.create(3);
    assert(p.create(4) == nullptr);

    auto* addr_b = b;
    p.destroy(b);
    auto* d = p.create(4);
    assert(d != nullptr && d == addr_b);

    p.destroy(a);
    p.destroy(c);
    p.destroy(d);
    }

    {
    TestSection section{"Destructor Called On Destroy"};
    g_events.clear();
    pool::PoolAllocator<Tracked, 1> p;
    auto* a = p.create(42);
    assert(g_events[0] == 42);
    p.destroy(a);
    assert(g_events[1] == -42);
    }

    {
    TestSection section{"Constructor Error Releases Slot"};
    struct Thrower {
        explicit Thrower(bool will_throw) {
            if (will_throw) throw std::runtime_error("[ERROR] Object could not be constructed");
        }
    };
    pool::PoolAllocator<Thrower, 1> p;
    bool constructor_failed = false;
    try { (void)p.create(true); } catch (const std::runtime_error&) { constructor_failed = true; }
    assert(constructor_failed);
    auto* t = p.create(false);
    assert(t != nullptr);
    p.destroy(t);
    }

    {
    TestSection section{"Fill, Empty, and then Refill"};
    constexpr std::size_t N = 32;
    pool::PoolAllocator<int, N> p;
    for (int round = 0; round < 2; ++round) {
        std::set<int*> seen;
        for (std::size_t i = 0; i < N; ++i) {
            int* x = p.create(static_cast<int>(i));
            assert(x != nullptr);
            assert(seen.insert(x).second);
        }
        assert(p.create(0) == nullptr);
        for (int* x : seen) p.destroy(x);
    }
    }


    return 0;
}

