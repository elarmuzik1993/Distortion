#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "ReportStore.h"
#include "Report.h"
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

        // Synchronous core: claim -> POST -> remove (success) / revert (failure). Returns
        // #sent. When allowAutoReports is false (consent revoked), pending 'auto' reports
        // are purged unsent; 'user' reports always send (the Send click was their consent).
        int drainOnce (int maxSends, bool allowAutoReports = true);

        // Async: kick a single background drain (no-op if one is already running, or if
        // another process holds the inter-process lock). Call from the message thread only.
        // Pass the current consent state so opted-out 'auto' reports are purged, not sent.
        void requestDrain (bool allowAutoReports = true);

    private:
        void run() override;

        ReportStore&  store;
        ITransport&   transport;
        juce::String  endpoint;
        std::atomic<bool> drainAllowsAuto { true };   // snapshot for the background run()
    };
}
