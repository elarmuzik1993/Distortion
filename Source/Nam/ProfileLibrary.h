#pragma once

#include <JuceHeader.h>
#include <functional>

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

    struct Position
    {
        int index = -1;   // zero-based; -1 when the file is not in the library
        int count = 0;
    };
    Position positionOf(const juce::File& file) const;

    // The profile `delta` steps away from `current`, wrapping at both ends. From a
    // file outside the library (or none), forward starts at the first profile and
    // back at the last. An invalid File when the library is empty. `where`, when
    // given, receives the result's position, from the same read of the folder.
    juce::File step(const juce::File& current, int delta, Position* where = nullptr) const;

    // Copies `source` into the top of the library and returns the copy. A file
    // already in the library comes back as it is. A same-named file with the same
    // content is reused; different content gets " (2)", " (3)" and so on. Returns
    // an invalid File and fills errorOut when the copy fails.
    juce::File import(const juce::File& source, juce::String& errorOut) const;

    // Imports several files, as "Load profile..." does. `check` vets each one first
    // and returns why it isn't a usable profile, naming the file (empty when it
    // is); a file it rejects stays out of the library. A good file the library
    // can't take is still listed, from where it is, so it can play, and the
    // problem is reported.
    struct ImportResult
    {
        juce::Array<juce::File> loadable;   // in the order given; the library's copies where they could be made
        juce::StringArray problems;         // one line per file skipped or left outside the library
    };
    ImportResult importAll(const juce::Array<juce::File>& sources,
                           const std::function<juce::String(const juce::File&)>& check) const;

    // What a drag-and-drop of `paths` offers as profiles: each .nam file as given,
    // then each folder's .nam files (subfolders included, natural order by relative
    // path, hidden files skipped). Anything else is ignored; duplicates appear once.
    static juce::Array<juce::File> collectProfiles(const juce::StringArray& paths);
    // Whether a dragged path could hold a profile: a .nam file or a folder.
    static bool mayHoldProfiles(const juce::String& path);

    // Identifies a profile's content independently of where it lives: its size
    // and a 64-bit FNV-1a hash of its bytes, "<size>-<hash>". Empty if unreadable.
    static juce::String fingerprint(const juce::File& file);

    // A library file with this fingerprint, preferring one named `preferredName`.
    // Lets a session whose profile was moved find the library's copy. Only files
    // of the right size are hashed. An invalid File when there is none.
    juce::File findByFingerprint(const juce::String& fingerprint,
                                 const juce::String& preferredName = {}) const;

private:
    juce::File root;
};
