#pragma once
#include <JuceHeader.h>
#include "Report.h"
#include "DiagnosticsSink.h"

namespace diag
{
    class ReportComposer
    {
    public:
        static Report compose (const juce::String& trigger,
                               const juce::String& message,
                               const DiagnosticsSink::Summary& anomalies,
                               double sampleRate, int blockSize,
                               juce::AudioProcessor::WrapperType wrapper,
                               const juce::String& installId);

        // Reads the install id from idFile; if absent, generates + writes one atomically.
        static juce::String loadOrCreateInstallId (const juce::File& idFile);
    };
}
