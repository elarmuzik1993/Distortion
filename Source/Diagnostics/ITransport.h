#pragma once
#include <JuceHeader.h>

namespace diag
{
    struct TransportResult
    {
        bool success = false;   // true iff the server accepted the report (2xx)
        int  statusCode = 0;
    };

    // Blocking POST. Implementations MUST honor timeoutMs and never touch the audio thread.
    class ITransport
    {
    public:
        virtual ~ITransport() = default;
        virtual TransportResult post (const juce::String& url,
                                      const juce::String& jsonBody,
                                      int timeoutMs) = 0;
    };
}
