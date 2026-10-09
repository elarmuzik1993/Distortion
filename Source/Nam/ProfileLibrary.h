#pragma once

#include <JuceHeader.h>

// The user's NAM profile library: a folder Sledge owns, next to Presets, that the
// browser arrows step through. A profile loaded from anywhere else is copied in
// first, so a session points at the library's copy and keeps working when the
// original is moved or deleted.
//
// Every call reads the folder afresh, so files added or removed in Explorer or
// Finder show up on the next step. Reads and copies files: never call it from
// the audio thread.
class ProfileLibrary
{
public:
    explicit ProfileLibrary(juce::File rootFolder) : root(std::move(rootFolder)) {}

    // <app data>/Monolit Beatz/Sledge Distortion/Profiles
    static juce::File defaultRoot();

    const juce::File& getRoot() const noexcept { return root; }

    // Every .nam file under the root, subfolders included, in natural order of
    // their paths relative to the root ("amp 2" before "amp 10").
    juce::Array<juce::File> list() const;

    bool contains(const juce::File& file) const;

    // The profile `delta` steps away from `current`, wrapping at both ends. From a
    // file outside the library (or none), forward starts at the first profile and
    // back at the last. An invalid File when the library is empty.
    juce::File step(const juce::File& current, int delta) const;

    struct Position
    {
        int index = -1;   // zero-based; -1 when the file is not in the library
        int count = 0;
    };
    Position positionOf(const juce::File& file) const;

    // Copies `source` into the top of the library and returns the copy. A file
    // already in the library comes back as it is. A same-named file with the same
    // content is reused; different content gets " (2)", " (3)" and so on. Returns
    // an invalid File and fills errorOut when the copy fails.
    juce::File import(const juce::File& source, juce::String& errorOut) const;

    // Identifies a profile's content independently of where it lives: its size
    // and a 64-bit FNV-1a hash of its bytes, "<size>-<hash>". Empty if unreadable.
    static juce::String fingerprint(const juce::File& file);

    // A library file with this fingerprint, preferring one named `preferredName`.
    // Lets a session whose profile was moved find the library's copy. Only files
    // of the right size are hashed. An invalid File when there is none.
    juce::File findByFingerprint(const juce::String& fingerprint,
                                 const juce::String& preferredName = {}) const;

    // The file name ending a path saved on any OS: "amp.nam" from "C:\Profiles\amp.nam"
    // as from "/Users/me/amp.nam". A session from another OS holds a path that
    // juce::File can't parse here.
    static juce::String fileNameOf(const juce::String& path)
    {
        return path.fromLastOccurrenceOf("/", false, false).fromLastOccurrenceOf("\\", false, false);
    }

private:
    juce::File root;
};
