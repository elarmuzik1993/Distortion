#pragma once
#include <JuceHeader.h>
#include "ITransport.h"
#include "Ping.h"

namespace diag
{
    // Fires at most one best-effort POST per day, on a short-lived background thread.
    // Unlike ReportSender there is no on-disk queue: a dropped ping (offline launch,
    // POST failure) isn't worth retrying mid-session -- the marker file is only updated
    // on a successful (2xx) response, so a failed attempt naturally retries next launch.
    class PingSender : private juce::Thread
    {
    public:
        // Per-POST network timeout, matching ReportSender's budget.
        static constexpr int timeoutMs    = 3000;
        // Teardown stop budget. MUST exceed timeoutMs so an in-flight POST completes and
        // the thread exits cleanly instead of being killed mid I/O.
        static constexpr int stopThreadMs = timeoutMs + 2000;

        PingSender (ITransport& transportToUse, juce::String endpointUrl, juce::File markerFileToUse);
        ~PingSender() override;

        static juce::String todayUtc();   // "YYYY-MM-DD"

        // Synchronous core: skip if the marker file already holds `today`; otherwise POST
        // `ping` and, only on success, write `today` to the marker file. Returns true iff
        // a POST was attempted (regardless of its outcome).
        bool sendIfDue (const Ping& ping, const juce::String& today);

        // Async: snapshot `ping` and run sendIfDue(ping, todayUtc()) on a background
        // thread. No-op if a send is already in flight. Call from the message thread
        // only, once at launch.
        void requestSend (const Ping& ping);

    private:
        void run() override;

        ITransport&   transport;
        juce::String  endpoint;
        juce::File    markerFile;
        Ping          pendingPing;   // snapshot taken by requestSend() for run()
    };
}
