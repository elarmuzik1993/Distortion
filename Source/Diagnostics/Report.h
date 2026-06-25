#pragma once
#include <JuceHeader.h>

namespace diag
{
    struct Report
    {
        int           schema = 1;
        juce::String  trigger;          // "user" | "auto"
        juce::String  installId;
        juce::String  pluginVersion;
        juce::String  os;
        juce::String  hostWrapper;
        juce::String  hostName;
        double        sampleRate = 0.0;
        int           blockSize = 0;
        juce::uint32  nonFiniteBlocks = 0;
        juce::uint64  totalBlocks = 0;
        juce::String  message;          // user reports only
        juce::String  createdUtc;

        juce::String toJson() const
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("schema",        schema);
            o->setProperty ("trigger",       trigger);
            o->setProperty ("installId",     installId);
            o->setProperty ("pluginVersion", pluginVersion);
            o->setProperty ("os",            os);
            o->setProperty ("hostWrapper",   hostWrapper);
            o->setProperty ("hostName",      hostName);
            o->setProperty ("sampleRate",    sampleRate);
            o->setProperty ("blockSize",     blockSize);
            o->setProperty ("nonFiniteBlocks", (juce::int64) nonFiniteBlocks);
            o->setProperty ("totalBlocks",     (juce::int64) totalBlocks);  // juce::var stores int64; counter won't realistically exceed INT64_MAX
            o->setProperty ("message",       message);
            o->setProperty ("createdUtc",    createdUtc);
            return juce::JSON::toString (juce::var (o.get()));
        }

        static Report fromJson (const juce::String& json, bool& ok)
        {
            Report r;
            juce::var v;
            auto result = juce::JSON::parse (json, v);
            if (result.failed() || ! v.isObject()) { ok = false; return r; }
            auto* o = v.getDynamicObject();
            if (o == nullptr) { ok = false; return r; }
            r.schema        = (int) o->getProperty ("schema");
            r.trigger       = o->getProperty ("trigger").toString();
            r.installId     = o->getProperty ("installId").toString();
            r.pluginVersion = o->getProperty ("pluginVersion").toString();
            r.os            = o->getProperty ("os").toString();
            r.hostWrapper   = o->getProperty ("hostWrapper").toString();
            r.hostName      = o->getProperty ("hostName").toString();
            r.sampleRate    = (double) o->getProperty ("sampleRate");
            r.blockSize     = (int) o->getProperty ("blockSize");
            r.nonFiniteBlocks = (juce::uint32) (juce::int64) o->getProperty ("nonFiniteBlocks");
            r.totalBlocks     = (juce::uint64) (juce::int64) o->getProperty ("totalBlocks");
            r.message       = o->getProperty ("message").toString();
            r.createdUtc    = o->getProperty ("createdUtc").toString();
            ok = true;
            return r;
        }
    };
}
