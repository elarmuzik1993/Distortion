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

    int ReportSender::drainOnce (int maxSends, bool allowAutoReports)
    {
        int sent = 0;
        for (auto& pending : store.listPending())
        {
            if (sent >= maxSends || threadShouldExit())
                break;

            const auto json = pending.loadFileAsString();
            if (json.isEmpty())
                continue;   // unreadable / mid-write; retry next launch

            // Consent revoked between sessions: purge pending 'auto' reports unsent.
            // 'user' reports always send — the Send click was their consent.
            if (! allowAutoReports)
            {
                bool ok = false;
                const auto report = Report::fromJson (json, ok);
                if (ok && report.trigger == "auto")
                {
                    store.remove (pending);
                    continue;
                }
            }

            const auto claimed = store.claim (pending);
            if (! claimed.existsAsFile())
                continue;   // another drainer took it

            const auto res = transport.post (endpoint, json, timeoutMs);
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

    void ReportSender::requestDrain (bool allowAutoReports)
    {
        if (! isThreadRunning())
        {
            drainAllowsAuto.store (allowAutoReports);
            startThread();
        }
    }

    void ReportSender::run()
    {
        // Single drainer across all instances AND processes (Standalone + VST3).
        juce::InterProcessLock lock ("MonolitDistortionReportDrain");
        if (! lock.enter (0))     // try-lock, no wait; another instance/process is draining
            return;

        store.recoverStaleClaims();
        drainOnce (maxSendsPerLaunch, drainAllowsAuto.load());
        lock.exit();
    }
}
