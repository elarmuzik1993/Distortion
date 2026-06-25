#pragma once
#include <atomic>
#include <cstdint>

namespace diag
{
    // Lock-free anomaly counters. Audio thread only INCREMENTS (noteBlock);
    // message thread reads snapshot()/reset(). No allocation, no locking.
    class DiagnosticsSink
    {
    public:
        struct Summary { std::uint32_t nonFiniteBlocks = 0; std::uint64_t totalBlocks = 0; };

        void noteBlock (bool finite) noexcept
        {
            total.fetch_add (1, std::memory_order_relaxed);
            if (! finite)
                nonFinite.fetch_add (1, std::memory_order_relaxed);
        }

        Summary snapshot() const noexcept
        {
            return { nonFinite.load (std::memory_order_relaxed),
                     total.load (std::memory_order_relaxed) };
        }

        bool hasAnomalies() const noexcept
        {
            return nonFinite.load (std::memory_order_relaxed) > 0;
        }

        void reset() noexcept
        {
            nonFinite.store (0, std::memory_order_relaxed);
            total.store (0, std::memory_order_relaxed);
        }

    private:
        std::atomic<std::uint32_t> nonFinite { 0 };
        std::atomic<std::uint64_t> total { 0 };
    };
}
