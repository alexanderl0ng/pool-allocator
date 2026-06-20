#include "../pool_allocator.hpp"

#include <vector>
#include <cassert>
#include <iostream>

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
    TestSection("General Allocation");
    pool::PoolAllocator<Tracked, 2> p;
    auto* a = p.create(1);
    auto* b = p.create(2);
    assert(a != nullptr && b != nullptr);
    auto* c = p.create(3);
    assert(c == nullptr);
    p.deallocate(a);
    p.deallocate(b);
    }

    {
    TestSection("Single Slot pool");
    pool::PoolAllocator<Tracked, 1> p;
    auto *a = p.create(1);
    assert(a != nullptr);
    assert(p.create(2) == nullptr);
    p.deallocate(a);
    auto* b = p.create(3);
    assert(b != nullptr);
    p.deallocate(b);
    }

    {
    TestSection("Alignment for Small Type");
    struct Tiny { char c; };
    pool::PoolAllocator<Tiny, 4> p;
    auto* a = p.create('a');
    auto addr = reinterpret_cast<std::uintptr_t>(a);
    assert(addr % alignof(void*) == 0);
    p.deallocate(a);
    }

    {
    TestSection("Non-LIFO Free Order");
    pool::PoolAllocator<Tracked, 3> p;
    auto* a = p.create(1);
    auto* b = p.create(2);
    auto* c = p.create(3);
    assert(p.create(4) == nullptr);

    auto* addr_b = b;
    p.deallocate(b);
    auto* d = p.create(4);
    assert(d != nullptr && d == addr_b);

    p.deallocate(a);
    p.deallocate(c);
    p.deallocate(d);
    }

    {
    TestSection("Destructor Called On Deallocate");
    g_events.clear();
    pool::PoolAllocator<Tracked, 1> p;
    auto* a = p.create(42);
    assert(g_events[0] == 42);
    p.deallocate(a);
    assert(g_events[1] == -42);
    }

    return 0;
}

