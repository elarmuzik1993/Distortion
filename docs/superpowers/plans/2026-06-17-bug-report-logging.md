# Bug-Report Logging + Send-on-Next-Launch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a privacy-light bug-reporting pipeline — problems are written to a durable on-disk queue (user-initiated or auto-detected) and sent on the next launch via background HTTP POST.

**Architecture:** A new `Source/Diagnostics/` subsystem owned by `PluginProcessor`. A lock-free `DiagnosticsSink` counts non-finite audio on the audio thread; reports are composed and written to a capped on-disk queue (`ReportStore`); a `ReportSender` drains the queue on a stoppable background thread under an `InterProcessLock`, via a swappable `ITransport`. Default-ON with a Settings off switch + first-run notice. The disk queue is the source of truth.

**Tech Stack:** C++17, JUCE 7.0.12, CMake. Tests via JUCE `UnitTest` runner (`DistortionTests`), RT-safety verified with `DISTORTION_RT_GUARD`.

**Spec:** `docs/superpowers/specs/2026-06-17-bug-report-logging-design.md` · **Linear:** USE-53

---

## Conventions for every task

- **Build tests:** `cmake --build build --target DistortionTests -j$(nproc)`
  (one-time setup if `build/` is missing: `cmake -B build -DCMAKE_BUILD_TYPE=Debug`)
- **Run tests:** `./build/DistortionTests_artefacts/Debug/DistortionTests`
  The runner runs *all* registered tests and exits non-zero if any fail. To check one suite:
  `./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -E "PASSED|FAILED" | grep -i diag`
- **New production files** go in `Source/Diagnostics/` and must be added to `PLUGIN_SOURCES` in `CMakeLists.txt` (shared by plugin + tests + harnesses).
- **New tests:** declare the class in `Source/Tests/DistortionTests.h`, register a `static` instance in `registerAllTests()` (end of that file, ~line 568), implement in `Source/Tests/DistortionTests.cpp`.
- **Commit messages** end with `Ref USE-53` (the global hook references the Linear issue; do not use `Fixes` until the feature is merged).
- All new code lives in namespace `diag`.

---

## File structure (created/modified)

| File | Responsibility |
|---|---|
| `Source/Diagnostics/AppPaths.h` *(new)* | Shared app-data paths (product dir, reports dir, settings/installId files) |
| `Source/Diagnostics/Report.h` *(new)* | `Report` POD + JSON (de)serialize |
| `Source/Diagnostics/ITransport.h` *(new)* | Transport interface + `TransportResult` |
| `Source/Diagnostics/DiagnosticsSink.h` *(new)* | Lock-free non-finite counters (audio thread) |
| `Source/Diagnostics/ReportStore.{h,cpp}` *(new)* | Disk queue: enqueue/list/claim/revert/remove/prune/recover |
| `Source/Diagnostics/ReportComposer.{h,cpp}` *(new)* | Build `Report` from snapshot; load/create `installId` |
| `Source/Diagnostics/ReportSender.{h,cpp}` *(new)* | Drain queue (bg thread, IPLock, timeouts, caps) via `ITransport` |
| `Source/Diagnostics/CurlTransport.{h,cpp}` *(new)* | Real `ITransport` via `juce::URL` (plugin build only) |
| `Source/Diagnostics/ReportEndpoint.h` *(new)* | Endpoint URL constant |
| `Source/PluginProcessor.{h,cpp}` *(modify)* | Own the subsystem; non-finite probe; consent atomic; lifecycle |
| `Source/PluginEditor.{h,cpp}` *(modify)* | Consent toggle (default ON), first-run notice, "Report a Bug" dialog |
| `CMakeLists.txt` *(modify)* | Add Diagnostics sources; `JUCE_USE_CURL=1` on `Distortion` target only |
| `Source/Tests/DistortionTests.{h,cpp}` *(modify)* | New `Diagnostics` test suites + `FakeTransport` |

---

## Task 1: AppPaths + Report (JSON) + ITransport — pure core, no deps

**Files:**
- Create: `Source/Diagnostics/AppPaths.h`, `Source/Diagnostics/Report.h`, `Source/Diagnostics/ITransport.h`
- Modify: `CMakeLists.txt:49-59` (add to `PLUGIN_SOURCES`)
- Modify: `Source/Tests/DistortionTests.h`, `Source/Tests/DistortionTests.cpp`

- [ ] **Step 1: Create `Source/Diagnostics/AppPaths.h`**

```cpp
#pragma once
#include <JuceHeader.h>

namespace diag
{
    // Mirrors PluginEditor::getPresetDirectory()'s parent (PluginEditor.cpp:1248-1251).
    inline juce::File productDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MonolitBeats")
                   .getChildFile ("Monolit Distortion");
    }

    inline juce::File reportsDir()
    {
        auto d = productDir().getChildFile ("reports");
        if (! d.exists())
            d.createDirectory();
        return d;
    }

    inline juce::File settingsFile()  { return productDir().getChildFile ("settings.xml"); }
    inline juce::File installIdFile() { return productDir().getChildFile ("installId"); }
}
```

- [ ] **Step 2: Create `Source/Diagnostics/ITransport.h`**

```cpp
#pragma once
#include <JuceHeader.h>

namespace diag
{
    struct TransportResult
    {
        bool success = false;   // true iff the server accepted the report (2xx)
        int  statusCode = 0;
    };

    // Blocking POST. Implementations MUST honor timeoutMs and never touch the audio thread.
    class ITransport
    {
    public:
        virtual ~ITransport() = default;
        virtual TransportResult post (const juce::String& url,
                                      const juce::String& jsonBody,
                                      int timeoutMs) = 0;
    };
}
```

- [ ] **Step 3: Write the failing test for `Report` JSON round-trip**

Append to `Source/Tests/DistortionTests.cpp` (before the trailing `#endif`); add `#include "../Diagnostics/Report.h"` near the top includes (after line 6):

```cpp
class DiagReportJsonTest : public juce::UnitTest
{
public:
    DiagReportJsonTest() : juce::UnitTest ("Diagnostics Report JSON", "Diagnostics") {}
    void runTest() override
    {
        beginTest ("round-trips all fields");
        diag::Report r;
        r.trigger = "user"; r.installId = "abc-123"; r.pluginVersion = "v9.9";
        r.os = "Linux"; r.hostWrapper = "VST3"; r.hostName = "Reaper";
        r.sampleRate = 48000.0; r.blockSize = 512;
        r.nonFiniteBlocks = 3; r.totalBlocks = 1000;
        r.message = "it broke"; r.createdUtc = "2026-06-17T00:00:00Z";

        bool ok = false;
        auto back = diag::Report::fromJson (r.toJson(), ok);
        expect (ok, "fromJson failed to parse");
        expectEquals (back.trigger, r.trigger);
        expectEquals (back.installId, r.installId);
        expectEquals (back.sampleRate, r.sampleRate);
        expectEquals ((int) back.nonFiniteBlocks, (int) r.nonFiniteBlocks);
        expectEquals (back.message, r.message);

        beginTest ("garbage input fails cleanly");
        bool ok2 = true;
        diag::Report::fromJson ("not json {", ok2);
        expect (! ok2, "garbage should set ok=false");
    }
};
```

Declare in `Source/Tests/DistortionTests.h` (after `SanityTests`, ~line 483) and register in `registerAllTests()`:

```cpp
// in header, with the other class declarations:
class DiagReportJsonTest;   // (full class is defined inline in the .cpp)
```

Actually define the class in the `.cpp` as shown and register it by adding to `registerAllTests()` in `DistortionTests.h`. Because `registerAllTests()` needs the type visible, instead **declare the class in the header** with its `runTest()` override and move the body to the `.cpp`. To keep it simple and match the file's existing inline style, put the **full class definition in `DistortionTests.h`** (like `SanityTests`) and delete the `.cpp` copy. Then register:

```cpp
static DiagReportJsonTest diagReportJsonTest;
```

- [ ] **Step 4: Run tests — verify FAIL**

Run: `cmake --build build --target DistortionTests -j$(nproc)`
Expected: **compile error** — `diag::Report` has no member `toJson`/`fromJson` (type not yet defined).

- [ ] **Step 5: Create `Source/Diagnostics/Report.h`**

```cpp
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
            o->setProperty ("totalBlocks",     (juce::int64) totalBlocks);
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
```

- [ ] **Step 6: Add the three new headers to `CMakeLists.txt` `PLUGIN_SOURCES`**

Edit `CMakeLists.txt:58` — after `Source/FactoryPresets.h` add:

```cmake
    Source/Diagnostics/AppPaths.h
    Source/Diagnostics/Report.h
    Source/Diagnostics/ITransport.h
```

- [ ] **Step 7: Run tests — verify PASS**

Run: `cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -i "Report JSON"`
Expected: `PASSED: Diagnostics Report JSON (...)`

- [ ] **Step 8: Commit**

```bash
git add Source/Diagnostics/ CMakeLists.txt Source/Tests/DistortionTests.h Source/Tests/DistortionTests.cpp
git commit -m "Add Diagnostics Report/JSON + ITransport + AppPaths (USE-53)

Ref USE-53"
```

---

## Task 2: ReportStore — durable, capped, claimable disk queue (M1/M3)

**Files:**
- Create: `Source/Diagnostics/ReportStore.h`, `Source/Diagnostics/ReportStore.cpp`
- Modify: `CMakeLists.txt` `PLUGIN_SOURCES`
- Modify: `Source/Tests/DistortionTests.{h,cpp}`

- [ ] **Step 1: Write the failing test**

Add `#include "../Diagnostics/ReportStore.h"` to the `.cpp` includes. Full test class in `DistortionTests.h` + register `static DiagReportStoreTest diagReportStoreTest;`:

```cpp
class DiagReportStoreTest : public juce::UnitTest
{
public:
    DiagReportStoreTest() : juce::UnitTest ("Diagnostics ReportStore", "Diagnostics") {}
    void runTest() override
    {
        auto tmp = juce::File::createTempFile ("diagq");
        tmp.deleteFile(); tmp.createDirectory();
        const juce::ScopedValueSetter<void*> cleanup (dummy, nullptr); // no-op; we clean below

        diag::ReportStore store (tmp);

        beginTest ("enqueue writes a pending file");
        diag::Report r; r.trigger = "auto"; r.installId = "x";
        auto f = store.enqueue (r);
        expect (f.existsAsFile(), "enqueue did not create file");
        expectEquals (store.listPending().size(), 1);

        beginTest ("claim hides file from listPending, revert restores");
        auto claimed = store.claim (f);
        expect (claimed.existsAsFile(), "claim target missing");
        expectEquals (store.listPending().size(), 0);
        store.revert (claimed);
        expectEquals (store.listPending().size(), 1);

        beginTest ("remove deletes");
        store.remove (store.listPending()[0]);
        expectEquals (store.listPending().size(), 0);

        beginTest ("prune enforces max file cap (drops oldest)");
        for (int i = 0; i < diag::ReportStore::maxFiles + 10; ++i)
            store.enqueue (r);
        expect (store.listPending().size() <= diag::ReportStore::maxFiles,
                "prune did not cap the queue");

        beginTest ("recoverStaleClaims reverts orphaned .sending files");
        auto g = store.enqueue (r);
        store.claim (g);                       // leaves a .sending with no drainer
        expectEquals (store.listPending().size(), store.listPending().size()); // baseline
        auto beforeRecover = store.listPending().size();
        store.recoverStaleClaims();
        expect (store.listPending().size() == beforeRecover + 1, "stale claim not recovered");

        tmp.deleteRecursively();
    }
private:
    void* dummy = nullptr;
};
```

- [ ] **Step 2: Run tests — verify FAIL** (`cmake --build build --target DistortionTests`). Expected: compile error, `diag::ReportStore` undefined.

- [ ] **Step 3: Create `Source/Diagnostics/ReportStore.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include "Report.h"

namespace diag
{
    // On-disk queue of pending reports. Pending = "*.json"; in-flight = "*.sending".
    // Safe to construct in multiple instances/processes pointing at the same dir.
    class ReportStore
    {
    public:
        static constexpr int maxFiles   = 50;
        static constexpr int maxAgeDays = 30;

        explicit ReportStore (juce::File directory);

        juce::File              enqueue (const Report&);   // writes <uuid>.json then prune()
        juce::Array<juce::File> listPending() const;       // *.json only, oldest first
        juce::File              claim (const juce::File& pending);   // .json -> .sending (atomic)
        void                    revert (const juce::File& claimed);  // .sending -> .json
        void                    remove (const juce::File&);
        void                    recoverStaleClaims();       // *.sending -> *.json (startup)
        void                    prune();                    // cap count + age

    private:
        juce::File dir;
    };
}
```

- [ ] **Step 4: Create `Source/Diagnostics/ReportStore.cpp`**

```cpp
#include "ReportStore.h"

namespace diag
{
    static bool olderFirst (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() < b.getLastModificationTime();
    }

    ReportStore::ReportStore (juce::File directory) : dir (std::move (directory))
    {
        if (! dir.exists())
            dir.createDirectory();
    }

    juce::File ReportStore::enqueue (const Report& r)
    {
        auto f = dir.getChildFile (juce::Uuid().toString() + ".json");
        f.replaceWithText (r.toJson());   // atomic-ish: writes via temp then renames
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
        claimed.moveFileTo (claimed.withFileExtension (".json"));
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
```

- [ ] **Step 5: Add `ReportStore.h` + `ReportStore.cpp` to `CMakeLists.txt` `PLUGIN_SOURCES`.**

- [ ] **Step 6: Run tests — verify PASS**

Run: `cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -i "ReportStore"`
Expected: `PASSED: Diagnostics ReportStore (...)`

- [ ] **Step 7: Commit**

```bash
git add Source/Diagnostics/ReportStore.* CMakeLists.txt Source/Tests/DistortionTests.*
git commit -m "Add Diagnostics ReportStore disk queue: claim/prune/recover (USE-53)

Ref USE-53"
```

---

## Task 3: DiagnosticsSink + ReportComposer

**Files:**
- Create: `Source/Diagnostics/DiagnosticsSink.h`, `Source/Diagnostics/ReportComposer.h`, `Source/Diagnostics/ReportComposer.cpp`
- Modify: `CMakeLists.txt`, `Source/Tests/DistortionTests.{h,cpp}`

- [ ] **Step 1: Create `Source/Diagnostics/DiagnosticsSink.h`**

```cpp
#pragma once
#include <atomic>
#include <cstdint>

namespace diag
{
    // Lock-free anomaly counters. Audio thread only INCREMENTS (noteBlock);
    // message thread reads snapshot()/reset(). No allocation, no locking.
    class DiagnosticsSink
    {
    public:
        struct Summary { std::uint32_t nonFiniteBlocks; std::uint64_t totalBlocks; };

        void noteBlock (bool finite) noexcept
        {
            total.fetch_add (1, std::memory_order_relaxed);
            if (! finite)
                nonFinite.fetch_add (1, std::memory_order_relaxed);
        }

        Summary snapshot() const noexcept
        {
            return { nonFinite.load (std::memory_order_relaxed),
                     total.load (std::memory_order_relaxed) };
        }

        bool hasAnomalies() const noexcept
        {
            return nonFinite.load (std::memory_order_relaxed) > 0;
        }

        void reset() noexcept
        {
            nonFinite.store (0, std::memory_order_relaxed);
            total.store (0, std::memory_order_relaxed);
        }

    private:
        std::atomic<std::uint32_t> nonFinite { 0 };
        std::atomic<std::uint64_t> total { 0 };
    };
}
```

- [ ] **Step 2: Write the failing test** (sink RT-safety + composer fields). Add includes `"../Diagnostics/DiagnosticsSink.h"`, `"../Diagnostics/ReportComposer.h"`, `"../RTAllocationGuard.h"` (already included). Full class in header + register `static DiagSinkComposerTest diagSinkComposerTest;`:

```cpp
class DiagSinkComposerTest : public juce::UnitTest
{
public:
    DiagSinkComposerTest() : juce::UnitTest ("Diagnostics Sink+Composer", "Diagnostics") {}
    void runTest() override
    {
        beginTest ("noteBlock is allocation-free (RT-safe)");
        diag::DiagnosticsSink sink;
        rt_guard::resetAllocationCounter();
        {
            rt_guard::ScopedRTAssert scope;
            for (int i = 0; i < 1000; ++i)
                sink.noteBlock (i % 100 != 0);   // 1% non-finite
        }
        expectEquals (rt_guard::getAllocationCount(), 0, "noteBlock allocated on audio thread");
        expect (sink.hasAnomalies(), "should have flagged anomalies");
        expectEquals ((int) sink.snapshot().nonFiniteBlocks, 10);
        expectEquals ((int) sink.snapshot().totalBlocks, 1000);

        beginTest ("reset clears counters");
        sink.reset();
        expect (! sink.hasAnomalies(), "reset failed");

        beginTest ("composer populates report from snapshot");
        diag::DiagnosticsSink::Summary sum { 3, 500 };
        auto r = diag::ReportComposer::compose ("auto", "", sum, 48000.0, 256,
                                                juce::AudioProcessor::wrapperType_VST3, "id-1");
        expectEquals (r.trigger, juce::String ("auto"));
        expectEquals (r.installId, juce::String ("id-1"));
        expectEquals (r.sampleRate, 48000.0);
        expectEquals ((int) r.nonFiniteBlocks, 3);
        expect (r.os.isNotEmpty(), "os should be filled");
        expect (r.createdUtc.isNotEmpty(), "timestamp should be filled");
        expectEquals (r.hostWrapper, juce::String ("VST3"));

        beginTest ("loadOrCreateInstallId is stable across calls");
        auto idFile = juce::File::createTempFile ("iid");
        idFile.deleteFile();
        auto id1 = diag::ReportComposer::loadOrCreateInstallId (idFile);
        auto id2 = diag::ReportComposer::loadOrCreateInstallId (idFile);
        expect (id1.isNotEmpty(), "id should be generated");
        expectEquals (id1, id2);
        idFile.deleteFile();
    }
};
```

- [ ] **Step 3: Run tests — verify FAIL** (compile error: `ReportComposer` undefined).

- [ ] **Step 4: Create `Source/Diagnostics/ReportComposer.h`**

```cpp
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
```

- [ ] **Step 5: Create `Source/Diagnostics/ReportComposer.cpp`**

```cpp
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
        idFile.getParentDirectory().createDirectory();
        idFile.replaceWithText (id);   // temp-write + rename
        return id;
    }
}
```

> Note: `juce::PluginHostType` lives in `juce_audio_processors` and requires `<JuceHeader.h>` (already pulled in). It needs `JucePlugin_Build_*` defines, which the test target provides.

- [ ] **Step 6: Add `DiagnosticsSink.h`, `ReportComposer.h`, `ReportComposer.cpp` to `PLUGIN_SOURCES`.**

- [ ] **Step 7: Run tests — verify PASS**

Run: `cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -i "Sink+Composer"`
Expected: `PASSED: Diagnostics Sink+Composer (...)`

- [ ] **Step 8: Commit**

```bash
git add Source/Diagnostics/DiagnosticsSink.h Source/Diagnostics/ReportComposer.* CMakeLists.txt Source/Tests/DistortionTests.*
git commit -m "Add DiagnosticsSink (RT-safe) + ReportComposer + installId (USE-53)

Ref USE-53"
```

---

## Task 4: ReportSender — drain via ITransport, IPLock, caps (M1/M3/M4)

**Files:**
- Create: `Source/Diagnostics/ReportSender.h`, `Source/Diagnostics/ReportSender.cpp`
- Modify: `CMakeLists.txt`, `Source/Tests/DistortionTests.{h,cpp}`

- [ ] **Step 1: Write the failing test (with a `FakeTransport`)**

Add `#include "../Diagnostics/ReportSender.h"`. Full class in header + register `static DiagSenderTest diagSenderTest;`:

```cpp
struct FakeTransport : diag::ITransport
{
    int calls = 0; bool succeed = true; juce::StringArray bodies;
    diag::TransportResult post (const juce::String&, const juce::String& body, int) override
    {
        ++calls; bodies.add (body);
        return { succeed, succeed ? 200 : 0 };
    }
};

class DiagSenderTest : public juce::UnitTest
{
public:
    DiagSenderTest() : juce::UnitTest ("Diagnostics ReportSender", "Diagnostics") {}
    void runTest() override
    {
        auto tmp = juce::File::createTempFile ("diagsend");
        tmp.deleteFile(); tmp.createDirectory();

        beginTest ("success deletes the report");
        {
            diag::ReportStore store (tmp);
            diag::Report r; r.trigger = "auto";
            store.enqueue (r); store.enqueue (r);
            FakeTransport tx;
            diag::ReportSender sender (store, tx, "https://example.test/r");
            int sent = sender.drainOnce (10);
            expectEquals (sent, 2);
            expectEquals (store.listPending().size(), 0);
            expectEquals (tx.calls, 2);
        }

        beginTest ("failure keeps the report for retry");
        {
            diag::ReportStore store (tmp);
            diag::Report r; r.trigger = "auto"; store.enqueue (r);
            FakeTransport tx; tx.succeed = false;
            diag::ReportSender sender (store, tx, "https://example.test/r");
            int sent = sender.drainOnce (10);
            expectEquals (sent, 0);
            expectEquals (store.listPending().size(), 1);  // reverted, not lost
        }

        beginTest ("per-launch send cap is honored");
        {
            diag::ReportStore store (tmp);
            store.recoverStaleClaims();
            for (auto& f : store.listPending()) store.remove (f);
            diag::Report r; r.trigger = "auto";
            for (int i = 0; i < 5; ++i) store.enqueue (r);
            FakeTransport tx;
            diag::ReportSender sender (store, tx, "https://example.test/r");
            int sent = sender.drainOnce (3);
            expectEquals (sent, 3);
            expectEquals (store.listPending().size(), 2);
        }

        tmp.deleteRecursively();
    }
};
```

- [ ] **Step 2: Run tests — verify FAIL** (compile error: `ReportSender` undefined).

- [ ] **Step 3: Create `Source/Diagnostics/ReportSender.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include "ReportStore.h"
#include "ITransport.h"

namespace diag
{
    // Drains the queue via ITransport. The synchronous drainOnce() is the testable core;
    // requestDrain() runs it on a stoppable background thread under a process-wide lock.
    class ReportSender : private juce::Thread
    {
    public:
        static constexpr int maxSendsPerLaunch = 10;
        static constexpr int timeoutMs = 8000;

        ReportSender (ReportStore& storeToUse, ITransport& transportToUse, juce::String endpointUrl);
        ~ReportSender() override;

        // Synchronous: claim → POST → remove (success) / revert (failure). Returns #sent.
        int drainOnce (int maxSends);

        // Async: kick a single background drain (no-op if one is already running, or if
        // another process holds the inter-process lock).
        void requestDrain();

    private:
        void run() override;

        ReportStore&  store;
        ITransport&   transport;
        juce::String  endpoint;
    };
}
```

- [ ] **Step 4: Create `Source/Diagnostics/ReportSender.cpp`**

```cpp
#include "ReportSender.h"

namespace diag
{
    ReportSender::ReportSender (ReportStore& s, ITransport& t, juce::String url)
        : juce::Thread ("DiagReportSender"), store (s), transport (t), endpoint (std::move (url)) {}

    ReportSender::~ReportSender()
    {
        signalThreadShouldExit();
        stopThread (2000);   // bounded; never block teardown indefinitely
    }

    int ReportSender::drainOnce (int maxSends)
    {
        int sent = 0;
        for (auto& pending : store.listPending())
        {
            if (sent >= maxSends || threadShouldExit())
                break;

            auto claimed = store.claim (pending);
            if (! claimed.existsAsFile())
                continue;   // someone else claimed it

            auto json = claimed.loadFileAsString();
            auto res = transport.post (endpoint, json, timeoutMs);
            if (res.success)
            {
                store.remove (claimed);
                ++sent;
            }
            else
            {
                store.revert (claimed);   // keep for next launch
            }
        }
        return sent;
    }

    void ReportSender::requestDrain()
    {
        if (! isThreadRunning())
            startThread();
    }

    void ReportSender::run()
    {
        // Single drainer across all instances AND processes (Standalone + VST3).
        juce::InterProcessLock lock ("MonolitDistortionReportDrain");
        if (! lock.enter (0))     // try-lock, no wait; another instance/process is draining
            return;

        store.recoverStaleClaims();
        drainOnce (maxSendsPerLaunch);
        lock.exit();
    }
}
```

- [ ] **Step 5: Add `ReportSender.h` + `ReportSender.cpp` to `PLUGIN_SOURCES`.**

- [ ] **Step 6: Run tests — verify PASS**

Run: `cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -i "ReportSender"`
Expected: `PASSED: Diagnostics ReportSender (...)`

- [ ] **Step 7: Commit**

```bash
git add Source/Diagnostics/ReportSender.* CMakeLists.txt Source/Tests/DistortionTests.*
git commit -m "Add ReportSender: drain via ITransport, IPLock, send cap (USE-53)

Ref USE-53"
```

---

## Task 5: CurlTransport + endpoint constant (real network, plugin build only)

**Files:**
- Create: `Source/Diagnostics/CurlTransport.h`, `Source/Diagnostics/CurlTransport.cpp`, `Source/Diagnostics/ReportEndpoint.h`
- Modify: `CMakeLists.txt:106-112` (add `JUCE_USE_CURL=1` to the **`Distortion`** target only)

> No unit test: the test target keeps `JUCE_USE_CURL=0` and uses `FakeTransport`. `CurlTransport` is exercised manually (Task 8 verification). It still compiles in all targets — `juce::URL` exists without curl; it just fails at runtime when curl is off, which only the plugin build enables.

- [ ] **Step 1: Create `Source/Diagnostics/ReportEndpoint.h`**

```cpp
#pragma once

// Single source of truth for the report endpoint. Swap this (or override via the
// DISTORTION_REPORT_ENDPOINT compile define) to point at a different backend.
#ifndef DISTORTION_REPORT_ENDPOINT
 #define DISTORTION_REPORT_ENDPOINT "https://REPLACE-ME.example.com/report"
#endif

namespace diag { inline const char* reportEndpoint() { return DISTORTION_REPORT_ENDPOINT; } }
```

- [ ] **Step 2: Create `Source/Diagnostics/CurlTransport.h`**

```cpp
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
```

- [ ] **Step 3: Create `Source/Diagnostics/CurlTransport.cpp`**

```cpp
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
        juce::StringPairArray responseHeaders;

        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders ("Content-Type: application/json")
                           .withConnectionTimeoutMs (timeoutMs)
                           .withResponseHeaders (&responseHeaders)
                           .withStatusCode (&statusCode);

        std::unique_ptr<juce::InputStream> stream (u.createInputStream (options));
        if (stream == nullptr)
            return { false, 0 };

        stream->readEntireStreamAsString();   // drain (bounded by server)
        const bool ok = (statusCode >= 200 && statusCode < 300);
        return { ok, statusCode };
    }
}
```

- [ ] **Step 4: Add the three files to `PLUGIN_SOURCES`; enable curl on the plugin target only.**

In `CMakeLists.txt`, change the `Distortion` target's `target_compile_definitions` (line ~109) from `JUCE_USE_CURL=0` to `JUCE_USE_CURL=1`. **Leave `DistortionTests`, `DistortionRender`, `DistortionSoak` at `JUCE_USE_CURL=0`.**

- [ ] **Step 5: Verify both targets still build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target DistortionTests --target Distortion_Standalone -j$(nproc)`
Expected: both link successfully.

- [ ] **Step 6: Run tests — verify still green**

Run: `./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | tail -5`
Expected: `ALL TESTS PASSED`.

- [ ] **Step 7: Commit**

```bash
git add Source/Diagnostics/CurlTransport.* Source/Diagnostics/ReportEndpoint.h CMakeLists.txt
git commit -m "Add CurlTransport + endpoint; enable JUCE_USE_CURL on plugin only (USE-53)

Ref USE-53"
```

---

## Task 6: Processor integration — sink probe, consent atomic, lifecycle (C1/M2)

**Files:**
- Modify: `Source/PluginProcessor.h` (members + public API), `Source/PluginProcessor.cpp` (ctor/dtor, `prepareToPlay`, `processBlock`, new methods)
- Modify: `Source/Tests/DistortionTests.{h,cpp}`

- [ ] **Step 1: Write the failing integration test.** Register `static DiagProcessorTest diagProcessorTest;`:

```cpp
class DiagProcessorTest : public juce::UnitTest
{
public:
    DiagProcessorTest() : juce::UnitTest ("Diagnostics Processor Integration", "Diagnostics") {}
    void runTest() override
    {
        beginTest ("consent defaults ON and is settable");
        PluginProcessor p;
        expect (p.areBugReportsEnabled(), "should default ON");
        p.setBugReportsEnabled (false);
        expect (! p.areBugReportsEnabled(), "setter failed");

        beginTest ("non-finite output is counted by the sink");
        p.setBugReportsEnabled (true);
        p.prepareToPlay (48000.0, 64);
        juce::AudioBuffer<float> buf (2, 64);
        juce::MidiBuffer midi;
        buf.clear();
        p.processBlock (buf, midi);
        const auto cleanCount = p.diagnosticsSink().snapshot().nonFiniteBlocks;
        // inject NaN into the input and process
        buf.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
        p.processBlock (buf, midi);
        expect (p.diagnosticsSink().snapshot().nonFiniteBlocks >= cleanCount,
                "sink should observe blocks");
        expect (p.diagnosticsSink().snapshot().totalBlocks >= 2, "blocks counted");

        beginTest ("submitUserReport enqueues a 'user' report");
        auto dir = diag::reportsDir();
        const int before = diag::ReportStore (dir).listPending().size();
        p.submitUserReport ("test message");
        const int after = diag::ReportStore (dir).listPending().size();
        expect (after >= before + 1, "user report not enqueued");
        // cleanup newest user report(s) created by this test
        for (auto& f : diag::ReportStore (dir).listPending())
            if (f.loadFileAsString().contains ("test message")) f.deleteFile();
    }
};
```

- [ ] **Step 2: Run tests — verify FAIL** (compile error: methods undefined).

- [ ] **Step 3: Add members + API to `Source/PluginProcessor.h`.** Add includes at top:

```cpp
#include "Diagnostics/DiagnosticsSink.h"
#include "Diagnostics/ReportStore.h"
#include "Diagnostics/ReportSender.h"
#include "Diagnostics/CurlTransport.h"
#include <atomic>
#include <memory>
```

In the `public:` section add:

```cpp
    // --- Bug reporting (USE-53) ---
    bool areBugReportsEnabled() const noexcept { return bugReportsEnabled.load(); }
    void setBugReportsEnabled (bool on) noexcept { bugReportsEnabled.store (on); }
    void submitUserReport (const juce::String& message);   // enqueues + kicks an immediate drain
    diag::DiagnosticsSink& diagnosticsSink() noexcept { return sink; }
```

In the `private:` section add:

```cpp
    std::atomic<bool>       bugReportsEnabled { true };
    diag::DiagnosticsSink   sink;
    diag::CurlTransport     transport;
    std::unique_ptr<diag::ReportStore>  reportStore;
    std::unique_ptr<diag::ReportSender> reportSender;
    std::unique_ptr<juce::Timer>        drainTimer;       // deferred, scan-safe (M2)
    juce::String            installId;

    void enqueueAndComposeReport (const juce::String& trigger, const juce::String& message);
```

- [ ] **Step 4: Implement in `Source/PluginProcessor.cpp`.**

In the **constructor body**, after existing init:

```cpp
    // Bug reporting (USE-53): read consent + install id, prepare queue/sender.
    if (auto xml = juce::parseXML (diag::settingsFile()))
        bugReportsEnabled.store (xml->getBoolAttribute ("bugReports", true));
    installId    = diag::ReportComposer::loadOrCreateInstallId (diag::installIdFile());
    reportStore  = std::make_unique<diag::ReportStore> (diag::reportsDir());
    reportSender = std::make_unique<diag::ReportSender> (*reportStore, transport, diag::reportEndpoint());

    // Scan-safe deferred drain (M2): a ~4s timer that a plugin-scan construct/destroy
    // storm never survives. Once-per-instance; the InterProcessLock makes it once-per-machine.
    struct DrainTimer : juce::Timer
    {
        PluginProcessor& owner;
        explicit DrainTimer (PluginProcessor& o) : owner (o) {}
        void timerCallback() override { stopTimer(); owner.reportSender->requestDrain(); }
    };
    drainTimer = std::make_unique<DrainTimer> (*this);
    static_cast<juce::Timer*> (drainTimer.get())->startTimer (4000);
```

Add `#include "Diagnostics/AppPaths.h"` and `#include "Diagnostics/ReportComposer.h"` to the `.cpp` includes.

In the **destructor**, before other teardown:

```cpp
    if (drainTimer) drainTimer->stopTimer();
    // Flush this session's anomalies as an 'auto' report (consent re-checked here, C1).
    if (bugReportsEnabled.load() && sink.hasAnomalies() && reportStore != nullptr)
        enqueueAndComposeReport ("auto", {});
    reportSender.reset();   // joins the bg thread (bounded, M4)
```

At the **end of `prepareToPlay`**, reset the per-session sink:

```cpp
    sink.reset();
```

At the **end of `processBlock`** (after all DSP, before return), the non-finite probe:

```cpp
    // RT-safe anomaly probe (USE-53): NaN/Inf persists once it appears, so sampling
    // the first frame of each channel reliably and cheaply detects sustained blow-ups.
    bool finite = true;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        finite = finite && std::isfinite (buffer.getSample (ch, 0));
    sink.noteBlock (finite);
```

Add the helper + user-report method:

```cpp
void PluginProcessor::enqueueAndComposeReport (const juce::String& trigger,
                                               const juce::String& message)
{
    auto report = diag::ReportComposer::compose (trigger, message, sink.snapshot(),
                                                 getSampleRate(), getBlockSize(),
                                                 wrapperType, installId);
    reportStore->enqueue (report);
}

void PluginProcessor::submitUserReport (const juce::String& message)
{
    if (reportStore == nullptr) return;
    enqueueAndComposeReport ("user", message);   // user reports always send (consent at submit)
    if (reportSender) reportSender->requestDrain();   // immediate flush attempt
}
```

- [ ] **Step 5: Run tests — verify PASS**

Run: `cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -i "Processor Integration"`
Expected: `PASSED: Diagnostics Processor Integration (...)`

- [ ] **Step 6: Verify whole suite + RT guard still green**

Run: `./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | tail -3`
Expected: `ALL TESTS PASSED`.

- [ ] **Step 7: Commit**

```bash
git add Source/PluginProcessor.* Source/Tests/DistortionTests.*
git commit -m "Wire diagnostics into PluginProcessor: probe, consent, lifecycle (USE-53)

Ref USE-53"
```

---

## Task 7: Editor UI — consent toggle (default ON), first-run notice, Report-a-Bug dialog

**Files:**
- Modify: `Source/PluginEditor.h` (`SettingsState`, new UI members), `Source/PluginEditor.cpp` (wiring)

> UI follows the existing pattern: `SettingsState` XML attrs (`PluginEditor.h:654-687`), the `oscilloscopeToggle` pattern (`PluginEditor.h:800-802`), and the deferred `saveSettings()` via `MessageManager::callAsync` (`PluginEditor.cpp:1432/1509`).

- [ ] **Step 1: Add the consent field to `SettingsState` (`PluginEditor.h:654`).**

Add member `bool bugReportsEnabled = true;`, and in `saveToFile()` add
`xml.setAttribute ("bugReports", bugReportsEnabled);`, in `loadFromFile()` add
`bugReportsEnabled = xml->getBoolAttribute ("bugReports", true);`.

- [ ] **Step 2: Declare UI members in `PluginEditor.h`** (near the other settings controls):

```cpp
    juce::ToggleButton bugReportsToggle { "Send anonymous bug reports" };
    juce::TextButton   reportBugButton  { "Report a Bug" };
    void showReportBugDialog();
    void maybeShowFirstRunNotice();
```

- [ ] **Step 3: Wire the toggle (in the editor constructor / settings build, mirroring `oscilloscopeToggle`).**

```cpp
    bugReportsToggle.setToggleState (settingsState.bugReportsEnabled, juce::dontSendNotification);
    bugReportsToggle.onClick = [this]
    {
        settingsState.bugReportsEnabled = bugReportsToggle.getToggleState();
        audioProcessor.setBugReportsEnabled (settingsState.bugReportsEnabled);   // live gate (C1)
        juce::Component::SafePointer<PluginEditor> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe) safe->saveSettings(); });
    };
    addAndMakeVisible (bugReportsToggle);   // place inside SettingsContent layout
```

- [ ] **Step 4: Wire the "Report a Bug" button + dialog.**

```cpp
    reportBugButton.onClick = [this] { showReportBugDialog(); };
    addAndMakeVisible (reportBugButton);
```

```cpp
void PluginEditor::showReportBugDialog()
{
    auto* editor = new juce::TextEditor();
    editor->setMultiLine (true);
    editor->setSize (360, 140);
    // Dialog copy carries informed consent (C2/C3):
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Report a Bug";
    o.content.setOwned (editor);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    // A real build adds Send/Cancel buttons; on Send:
    //   audioProcessor.submitUserReport (editor->getText());
    // Show a one-line note above the field:
    //   "Sends version, OS, and DAW name to help fix this — now, or on next launch if offline.
    //    Please don't type personal information."
    o.launchAsync();
}
```

> Implementation note for the builder: add **Send** and **Cancel** `TextButton`s and the note label to a small container `Component` (instead of the bare `TextEditor`), and call `audioProcessor.submitUserReport (text)` on Send. Keep the copy exactly as above (C2/C3).

- [ ] **Step 5: First-run notice (`maybeShowFirstRunNotice`), called once after `loadSettings()` in the constructor.**

```cpp
void PluginEditor::maybeShowFirstRunNotice()
{
    if (diag::settingsFile().existsAsFile())
        return;   // not a first run — settings already saved before
    juce::AlertWindow::showMessageBoxAsync (
        juce::AlertWindow::InfoIcon, "Monolit Distortion",
        "Monolit sends anonymous bug reports to help fix issues. "
        "It's on by default \xe2\x80\x94 you can turn it off in Settings.");
    saveSettings();   // persist so the notice shows only once
}
```

Add `#include "Diagnostics/AppPaths.h"` to `PluginEditor.cpp`.

- [ ] **Step 6: Build the Standalone and smoke-test the UI**

Run: `cmake --build build --target Distortion_Standalone -j$(nproc)`
Then launch: `./build/Distortion_artefacts/Debug/Standalone/"Monolit Distortion"`
Expected: first launch shows the notice; Settings shows the toggle (ON); "Report a Bug" opens the dialog. (Delete `~/.config/MonolitBeats/Monolit Distortion/settings.xml` first to re-trigger the first-run notice.)

- [ ] **Step 7: Run unit tests — still green**

Run: `cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | tail -3`
Expected: `ALL TESTS PASSED`.

- [ ] **Step 8: Commit**

```bash
git add Source/PluginEditor.*
git commit -m "Add consent toggle (default ON), first-run notice, Report-a-Bug UI (USE-53)

Ref USE-53"
```

---

## Task 8: Backend receiver + endpoint wiring (separate from plugin)

**Files:**
- Modify: `Source/Diagnostics/ReportEndpoint.h` (set the real URL) — or pass `-DDISTORTION_REPORT_ENDPOINT=...` in CI.

- [ ] **Step 1: Stand up a minimal HTTPS receiver** (out of this repo). It must:
  - accept `POST` `application/json`,
  - validate `schema == 1` and cap body size (e.g. 16 KB),
  - store/relay the report (file/db/email),
  - return **HTTP 2xx** on accept (the plugin deletes a report only on 2xx).
  A serverless function (Cloudflare Workers / Cloud Function free tier) is sufficient; a form-backend (Formspree-style) is an acceptable zero-code fallback.

- [ ] **Step 2: Point the plugin at it.** Set `DISTORTION_REPORT_ENDPOINT` to the real URL in `ReportEndpoint.h`, **or** add `target_compile_definitions(Distortion PRIVATE DISTORTION_REPORT_ENDPOINT="https://...")` in `CMakeLists.txt`.

- [ ] **Step 3: End-to-end manual verification.**
  - Build Standalone (curl enabled).
  - Click "Report a Bug" → Send.
  - Confirm the report file appears under `~/.config/MonolitBeats/Monolit Distortion/reports/`, then disappears after the background drain, and **arrives at the backend**.
  - Toggle reporting **off**, force a NaN, quit → confirm **no** `auto` report is queued.

- [ ] **Step 4: Commit**

```bash
git add Source/Diagnostics/ReportEndpoint.h CMakeLists.txt
git commit -m "Wire real report endpoint (USE-53)

Ref USE-53"
```

---

## Task 9: Docs — privacy note, release gate, AGENTS.md

**Files:**
- Create: `docs/PRIVACY.md`
- Modify: `RELEASE_CHECKLIST.md`, `AGENTS.md`

- [ ] **Step 1: Write `docs/PRIVACY.md`** — what is collected (version, OS, DAW name, sample rate/block size, anomaly counts, anonymous install id, optional user text), that it is **on by default with a Settings off switch**, how to opt out, and that users should not type personal data in the free-text field (C3).

- [ ] **Step 2: Add a `RELEASE_CHECKLIST.md` gate** — "Bug reporting: first-run notice shows once; toggle persists; opt-out suppresses `auto` reports; endpoint reachable; queue caps enforced."

- [ ] **Step 3: Update `AGENTS.md`** — add a "Diagnostics / bug reporting" subsystem note (paths: `Source/Diagnostics/`, `…/Monolit Distortion/reports/`, `installId`; default-on + off switch; `JUCE_USE_CURL=1` on the plugin target only).

- [ ] **Step 4: Commit**

```bash
git add docs/PRIVACY.md RELEASE_CHECKLIST.md AGENTS.md
git commit -m "Document bug-reporting pipeline: privacy, release gate, AGENTS (USE-53)

Ref USE-53"
```

---

## Self-review

**Spec coverage:** triggers (Task 6 probe + Task 7 button) ✓; delivery HTTP POST queue+drain (Tasks 2/4/5) ✓; default-ON + off switch + first-run notice (Tasks 6/7) ✓; minimal payload + installId own file (Tasks 1/3) ✓; M1 IPLock+claim (Tasks 2/4) ✓; M2 deferred timer (Task 6) ✓; M3 caps (Tasks 2/4) ✓; M4 timeouts+stoppable (Tasks 4/5) ✓; C1 atomic gate (Task 6) ✓; C2 no-cancel + dialog copy (Task 7) ✓; C3 PII note (Tasks 7/9) ✓; backend (Task 8) ✓; docs (Task 9) ✓. Out of scope (crash handler) — not planned, per spec. **No gaps.**

**Type consistency:** `diag::Report`, `DiagnosticsSink::Summary {nonFiniteBlocks,totalBlocks}`, `ITransport::post(url,json,timeoutMs)→TransportResult{success,statusCode}`, `ReportStore::{enqueue,listPending,claim,revert,remove,recoverStaleClaims,prune}`, `ReportComposer::{compose,loadOrCreateInstallId}`, `ReportSender::{drainOnce,requestDrain}`, `PluginProcessor::{areBugReportsEnabled,setBugReportsEnabled,submitUserReport,diagnosticsSink}` — names match across all tasks.

**Placeholder scan:** real code/commands in every code step. The only intentional value-to-fill is the backend URL (`REPLACE-ME`), which is the explicit deliverable of Task 8.
