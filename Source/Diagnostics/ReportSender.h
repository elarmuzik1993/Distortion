#pragma once
#include <JuceHeader.h>
#include "ReportStore.h"
#include "ITransport.h"

namespace diag
{
    // Drains the queue via ITransport. The synchronous drainOnce() is the testable core;
    // requestDrain() runs it on a stoppable background thread under a process-wide lock.
    class ReportSender : private juce::Thread
    {
    public:
        static constexpr int maxSendsPerLaunch = 10;
        // Per-POST network timeout. Kept short so a mid-flight POST can finish and the
        // drain loop can observe threadShouldExit() within the stopThread budget below.
        static constexpr int timeoutMs    = 3000;
        // Teardown stop budget. MUST exceed timeoutMs so an in-flight POST completes and
        // the thread exits cleanly, avoiding JUCE killThread() on a thread in network I/O
        // (which would leak the InterProcessLock fd in the host process).
        static constexpr int stopThreadMs = timeoutMs + 2000;

        ReportSender (ReportStore& storeToUse, ITransport& transportToUse, juce::String endpointUrl);
        ~ReportSender() override;

        // Synchronous: claim -> POST -> remove (success) / revert (failure). Returns #sent.
        int drainOnce (int maxSends);

        // Async: kick a single background drain (no-op if one is already running, or if
        // another process holds the inter-process lock). Call from the message thread only.
        void requestDrain();

    private:
        void run() override;

        ReportStore&  store;
        ITransport&   transport;
        juce::String  endpoint;
    };
}
