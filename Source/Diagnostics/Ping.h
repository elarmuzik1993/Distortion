#pragma once
#include <JuceHeader.h>
#include "../GitVersion.h"

namespace diag
{
    // Anonymous launch ping (USE-53 companion): the install/version/OS/host counterpart
    // to Report, sent at most once per install per day. No DSP fields -- there is no
    // anomaly data to attach, only "this install ran today".
    struct Ping
    {
        int           schema = 1;
        juce::String  installId;
        juce::String  pluginVersion;
        juce::String  os;
        juce::String  hostWrapper;
        juce::String  hostName;
        juce::String  createdUtc;

        juce::String toJson() const
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("schema",        schema);
            o->setProperty ("installId",     installId);
            o->setProperty ("pluginVersion", pluginVersion);
            o->setProperty ("os",            os);
            o->setProperty ("hostWrapper",   hostWrapper);
            o->setProperty ("hostName",      hostName);
            o->setProperty ("createdUtc",    createdUtc);
            return juce::JSON::toString (juce::var (o.get()));
        }

        static Ping compose (juce::AudioProcessor::WrapperType wrapper, const juce::String& installId)
        {
            Ping p;
            p.installId     = installId;
            p.pluginVersion = GIT_VERSION_STRING;
            p.os            = juce::SystemStats::getOperatingSystemName();
            p.hostWrapper   = juce::AudioProcessor::getWrapperTypeDescription (wrapper);
            p.hostName      = juce::PluginHostType().getHostDescription();
            p.createdUtc    = juce::Time::getCurrentTime().toISO8601 (true);
            return p;
        }
    };
}
