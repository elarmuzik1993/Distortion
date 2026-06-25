#pragma once
#include <JuceHeader.h>
#include "Report.h"

namespace diag
{
    // On-disk queue of pending reports. Pending = "*.json"; in-flight = "*.sending".
    // Safe to construct in multiple instances/processes pointing at the same dir.
    class ReportStore
    {
    public:
        static constexpr int maxFiles   = 50;
        static constexpr int maxAgeDays = 30;

        explicit ReportStore (juce::File directory);

        juce::File              enqueue (const Report&);   // writes <uuid>.json then prune()
        juce::Array<juce::File> listPending() const;       // *.json only, oldest first
        juce::File              claim (const juce::File& pending);   // .json -> .sending (atomic)
        void                    revert (const juce::File& claimed);  // .sending -> .json
        void                    remove (const juce::File&);
        void                    recoverStaleClaims();       // *.sending -> *.json (startup)
        void                    prune();                    // cap count + age

    private:
        juce::File dir;
    };
}
