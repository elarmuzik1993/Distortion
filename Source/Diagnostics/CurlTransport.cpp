#include "CurlTransport.h"

namespace diag
{
    TransportResult CurlTransport::post (const juce::String& url,
                                         const juce::String& jsonBody,
                                         int timeoutMs)
    {
        juce::URL u (url);
        u = u.withPOSTData (jsonBody);

        int statusCode = 0;

        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders ("Content-Type: application/json")
                           .withConnectionTimeoutMs (timeoutMs)
                           .withStatusCode (&statusCode);

        std::unique_ptr<juce::InputStream> stream (u.createInputStream (options));
        if (stream == nullptr)
            return { false, 0 };

        // statusCode is populated from the response header phase before the body is read.
        // Drain politely but cap it so a misbehaving endpoint can't balloon memory.
        juce::MemoryBlock drained;
        stream->readIntoMemoryBlock (drained, 8192);
        const bool ok = (statusCode >= 200 && statusCode < 300);
        return { ok, statusCode };
    }
}
