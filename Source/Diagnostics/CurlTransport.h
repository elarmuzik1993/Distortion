#pragma once
#include <JuceHeader.h>
#include "ITransport.h"

namespace diag
{
    // Real transport: HTTPS POST via juce::URL. Requires JUCE_USE_CURL=1 (plugin target).
    class CurlTransport : public ITransport
    {
    public:
        TransportResult post (const juce::String& url,
                              const juce::String& jsonBody,
                              int timeoutMs) override;
    };
}
