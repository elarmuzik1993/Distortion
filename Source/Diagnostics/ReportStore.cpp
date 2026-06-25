#include "ReportStore.h"
#include <algorithm>   // std::sort

namespace diag
{
    static bool olderFirst (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() < b.getLastModificationTime();
    }

    ReportStore::ReportStore (juce::File directory) : dir (std::move (directory))
    {
        if (! dir.exists())
        {
            auto result = dir.createDirectory();
            jassert (result.wasOk());
            juce::ignoreUnused (result);
        }
    }

    juce::File ReportStore::enqueue (const Report& r)
    {
        auto f = dir.getChildFile (juce::Uuid().toString() + ".json");
        f.replaceWithText (r.toJson());
        prune();
        return f;
    }

    juce::Array<juce::File> ReportStore::listPending() const
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        std::sort (files.begin(), files.end(), olderFirst);
        return files;
    }

    juce::File ReportStore::claim (const juce::File& pending)
    {
        auto target = pending.withFileExtension (".sending");
        if (pending.moveFileTo (target))
            return target;
        return {};
    }

    void ReportStore::revert (const juce::File& claimed)
    {
        const bool ok = claimed.moveFileTo (claimed.withFileExtension (".json"));
        jassert (ok);
        juce::ignoreUnused (ok);
    }

    void ReportStore::remove (const juce::File& f) { f.deleteFile(); }

    void ReportStore::recoverStaleClaims()
    {
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.sending"))
            f.moveFileTo (f.withFileExtension (".json"));
    }

    void ReportStore::prune()
    {
        auto files = listPending();
        // Age cap
        auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days (maxAgeDays);
        for (auto& f : files)
            if (f.getLastModificationTime() < cutoff)
                f.deleteFile();

        // Count cap (drop oldest)
        files = listPending();
        for (int i = 0; i < files.size() - maxFiles; ++i)
            files[i].deleteFile();
    }
}
