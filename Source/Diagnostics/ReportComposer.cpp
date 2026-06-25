#include "ReportComposer.h"
#include "../GitVersion.h"

namespace diag
{
    Report ReportComposer::compose (const juce::String& trigger,
                                    const juce::String& message,
                                    const DiagnosticsSink::Summary& anomalies,
                                    double sampleRate, int blockSize,
                                    juce::AudioProcessor::WrapperType wrapper,
                                    const juce::String& installId)
    {
        Report r;
        r.trigger        = trigger;
        r.message        = message;
        r.installId      = installId;
        r.pluginVersion  = GIT_VERSION_STRING;
        r.os             = juce::SystemStats::getOperatingSystemName();
        r.hostWrapper    = juce::AudioProcessor::getWrapperTypeDescription (wrapper);
        r.hostName       = juce::PluginHostType().getHostDescription();
        r.sampleRate     = sampleRate;
        r.blockSize      = blockSize;
        r.nonFiniteBlocks = anomalies.nonFiniteBlocks;
        r.totalBlocks     = anomalies.totalBlocks;
        r.createdUtc      = juce::Time::getCurrentTime().toISO8601 (true);
        return r;
    }

    juce::String ReportComposer::loadOrCreateInstallId (const juce::File& idFile)
    {
        if (idFile.existsAsFile())
        {
            auto existing = idFile.loadFileAsString().trim();
            if (existing.isNotEmpty())
                return existing;
        }
        auto id = juce::Uuid().toString();
        auto dirResult = idFile.getParentDirectory().createDirectory();
        jassert (dirResult.wasOk());
        juce::ignoreUnused (dirResult);
        if (! idFile.replaceWithText (id))   // temp-write + rename
        {
            // Returned id is ephemeral if this fails (will differ next launch).
            DBG ("ReportComposer: failed to persist install-id at " << idFile.getFullPathName());
        }
        return id;
    }
}
