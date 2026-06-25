#include "ReportSender.h"

namespace diag
{
    ReportSender::ReportSender (ReportStore& s, ITransport& t, juce::String url)
        : juce::Thread ("DiagReportSender"), store (s), transport (t), endpoint (std::move (url)) {}

    ReportSender::~ReportSender()
    {
        signalThreadShouldExit();
        stopThread (stopThreadMs);   // exceeds one POST timeout → clean exit, no killThread
    }

    int ReportSender::drainOnce (int maxSends)
    {
        int sent = 0;
        for (auto& pending : store.listPending())
        {
            if (sent >= maxSends || threadShouldExit())
                break;

            auto claimed = store.claim (pending);
            if (! claimed.existsAsFile())
                continue;   // someone else claimed it

            auto json = claimed.loadFileAsString();
            if (json.isEmpty())   // file vanished/unreadable between claim and read
            {
                store.revert (claimed);
                continue;
            }
            auto res = transport.post (endpoint, json, timeoutMs);
            if (res.success)
            {
                store.remove (claimed);
                ++sent;
            }
            else
            {
                store.revert (claimed);   // keep for next launch
            }
        }
        return sent;
    }

    void ReportSender::requestDrain()
    {
        if (! isThreadRunning())
            startThread();
    }

    void ReportSender::run()
    {
        // Single drainer across all instances AND processes (Standalone + VST3).
        juce::InterProcessLock lock ("MonolitDistortionReportDrain");
        if (! lock.enter (0))     // try-lock, no wait; another instance/process is draining
            return;

        store.recoverStaleClaims();
        drainOnce (maxSendsPerLaunch);
        lock.exit();
    }
}
