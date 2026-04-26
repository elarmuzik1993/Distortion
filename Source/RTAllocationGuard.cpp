#include "RTAllocationGuard.h"

#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD

#include <atomic>
#include <cstdlib>
#include <new>

namespace rt_guard
{
    // Per-thread depth counter so nested scopes do not turn the guard off
    // prematurely when an inner ScopedRTAssert destructs.
    static int& scopeDepth()
    {
        thread_local int depth = 0;
        return depth;
    }

    bool isActive() { return scopeDepth() > 0; }

    static std::atomic<int> counter { 0 };

    std::atomic<int>& allocationCounter()     { return counter; }
    void              resetAllocationCounter() { counter.store (0, std::memory_order_relaxed); }
    int               getAllocationCount()     { return counter.load (std::memory_order_relaxed); }

    ScopedRTAssert::ScopedRTAssert()  { ++scopeDepth(); }
    ScopedRTAssert::~ScopedRTAssert() { --scopeDepth(); }
}

// ---------------------------------------------------------------------------
// Global operator new/delete overrides.
// Only compiled into binaries built with DISTORTION_RT_GUARD defined (test
// runner only). The plugin binaries do not include this file in their source
// lists, so no override leaks into host processes.
// ---------------------------------------------------------------------------

static inline void countIfActive() noexcept
{
    if (rt_guard::isActive())
        rt_guard::allocationCounter().fetch_add (1, std::memory_order_relaxed);
}

void* operator new (std::size_t n)
{
    countIfActive();
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t n)
{
    countIfActive();
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}

void* operator new (std::size_t n, const std::nothrow_t&) noexcept
{
    countIfActive();
    return std::malloc (n == 0 ? 1 : n);
}

void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept
{
    countIfActive();
    return std::malloc (n == 0 ? 1 : n);
}

void operator delete   (void* p) noexcept                        { std::free (p); }
void operator delete[] (void* p) noexcept                        { std::free (p); }
void operator delete   (void* p, std::size_t) noexcept           { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept           { std::free (p); }
void operator delete   (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }

#endif // DISTORTION_RT_GUARD
