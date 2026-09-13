#include "PingSender.h"

namespace diag
{
    PingSender::PingSender (ITransport& transportToUse, juce::String endpointUrl, juce::File markerFileToUse)
        : juce::Thread ("diag::PingSender"),
          transport (transportToUse),
          endpoint (std::move (endpointUrl)),
          markerFile (std::move (markerFileToUse))
    {
    }

    PingSender::~PingSender()
    {
        stopThread (stopThreadMs);
    }

    juce::String PingSender::todayUtc()
    {
        return juce::Time::getCurrentTime().toISO8601 (true).substring (0, 10);
    }

    bool PingSender::sendIfDue (const Ping& ping, const juce::String& today)
    {
        auto last = markerFile.existsAsFile() ? markerFile.loadFileAsString().trim() : juce::String();
        if (last == today)
            return false;

        auto result = transport.post (endpoint, ping.toJson(), timeoutMs);
        if (result.success)
            markerFile.replaceWithText (today);
        return true;
    }

    void PingSender::requestSend (const Ping& ping)
    {
        if (isThreadRunning())
            return;
        pendingPing = ping;
        startThread();
    }

    void PingSender::run()
    {
        sendIfDue (pendingPing, todayUtc());
    }
}
