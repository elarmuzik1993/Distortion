#pragma once

/*
    RT-safety debug guard.

    Active only when DISTORTION_RT_GUARD is defined at compile time. That
    macro is set on the test target only (see CMakeLists.txt). Plugin builds
    expand RT_ASSERT_SCOPE() to a no-op and never link the cpp, so host
    processes are unaffected.

    Usage:
        void PluginProcessor::processBlock (...)
        {
            RT_ASSERT_SCOPE();
            ...
        }

    Tests then call rt_guard::resetAllocationCounter() before exercising a
    path and rt_guard::getAllocationCount() after to verify RT-safety.
*/

#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD

#include <atomic>

namespace rt_guard
{
    // Returns true iff the current thread is inside at least one active
    // ScopedRTAssert. Nesting-safe (depth-counted per thread).
    bool              isActive();

    std::atomic<int>& allocationCounter();
    void              resetAllocationCounter();
    int               getAllocationCount();

    class ScopedRTAssert
    {
    public:
        ScopedRTAssert();
        ~ScopedRTAssert();
        ScopedRTAssert (const ScopedRTAssert&)            = delete;
        ScopedRTAssert& operator= (const ScopedRTAssert&) = delete;
    };
}

#define RT_ASSERT_SCOPE() rt_guard::ScopedRTAssert _rt_assert_scope_instance

#else

#define RT_ASSERT_SCOPE() ((void) 0)

#endif
