#include "ProfileLibrary.h"
#include "../Diagnostics/AppPaths.h"

juce::File ProfileLibrary::defaultRoot()
{
    return diag::profilesDir();
}

juce::Array<juce::File> ProfileLibrary::list() const
{
    if (! root.isDirectory())
        return {};

    auto files = root.findChildFiles(juce::File::findFiles | juce::File::ignoreHiddenFiles, true,
                                     "*.nam", juce::File::FollowSymlinks::noCycles);

    struct ByRelativePath
    {
        const juce::File& base;
        int compareElements(const juce::File& a, const juce::File& b) const
        {
            return a.getRelativePathFrom(base).compareNatural(b.getRelativePathFrom(base));
        }
    };
    ByRelativePath order { root };
    files.sort(order);
    return files;
}

bool ProfileLibrary::contains(const juce::File& file) const
{
    return root != juce::File() && file.isAChildOf(root);
}

juce::File ProfileLibrary::step(const juce::File& current, int delta) const
{
    const auto files = list();
    if (files.isEmpty())
        return {};

    const int count = files.size();
    const int index = files.indexOf(current);
    if (index < 0)
        return delta >= 0 ? files.getFirst() : files.getLast();

    return files[((index + delta) % count + count) % count];
}

ProfileLibrary::Position ProfileLibrary::positionOf(const juce::File& file) const
{
    const auto files = list();
    return { files.indexOf(file), files.size() };
}

juce::File ProfileLibrary::import(const juce::File& source, juce::String& errorOut) const
{
    if (! source.existsAsFile())
    {
        errorOut = "Can't find " + source.getFileName();
        return {};
    }
    if (contains(source))
        return source;

    if (! root.isDirectory())
    {
        const auto created = root.createDirectory();
        if (created.failed())
        {
            errorOut = "Can't create the profile library: " + created.getErrorMessage();
            return {};
        }
    }

    const auto stem = source.getFileNameWithoutExtension();
    const auto extension = source.getFileExtension();
    for (int n = 1; n < 1000; ++n)
    {
        const auto target = root.getChildFile(n == 1 ? source.getFileName()
                                                     : stem + " (" + juce::String(n) + ")" + extension);
        if (target.existsAsFile())
        {
            if (target.hasIdenticalContentTo(source))
                return target;
            continue;
        }
        if (! source.copyFileTo(target))
        {
            errorOut = "Can't copy " + source.getFileName() + " into the profile library";
            return {};
        }
        return target;
    }

    errorOut = "Too many profiles named " + source.getFileName();
    return {};
}

juce::String ProfileLibrary::fingerprint(const juce::File& file)
{
    juce::FileInputStream in(file);
    if (! in.openedOk())
        return {};

    juce::uint64 hash = 14695981039346656037ull;   // FNV-1a 64 offset basis
    juce::HeapBlock<juce::uint8> chunk(65536);
    for (;;)
    {
        const int read = in.read(chunk.get(), 65536);
        if (read <= 0)
            break;
        for (int i = 0; i < read; ++i)
        {
            hash ^= chunk[i];
            hash *= 1099511628211ull;               // FNV-1a 64 prime
        }
    }
    return juce::String(file.getSize()) + "-" + juce::String::toHexString(static_cast<juce::int64>(hash));
}

juce::File ProfileLibrary::findByFingerprint(const juce::String& print, const juce::String& preferredName) const
{
    const auto size = print.upToFirstOccurrenceOf("-", false, false).getLargeIntValue();
    if (print.isEmpty() || size <= 0)
        return {};

    auto candidates = list();
    candidates.removeIf([size](const juce::File& f) { return f.getSize() != size; });

    // The same name first: almost always the one, and it saves hashing the rest.
    std::stable_partition(candidates.begin(), candidates.end(),
                          [&](const juce::File& f) { return f.getFileName() == preferredName; });

    for (const auto& candidate : candidates)
        if (fingerprint(candidate) == print)
            return candidate;
    return {};
}
