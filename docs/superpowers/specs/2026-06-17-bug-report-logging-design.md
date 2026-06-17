# Bug-Report Logging & Send-on-Next-Launch — Design / Roadmap

**Date:** 2026-06-17
**Status:** Approved direction; ready for implementation plan (rev 3 — default-ON + off switch & first-run notice; rev 2 — multi-instance/lifecycle M1–M4 + consent C1–C3)
**Component:** Diagnostics / bug reporting (new subsystem)
**Obsidian:** [[03 Projects/Monolit Distortion]]

---

## Context

Monolit Distortion ships VST3 + Standalone (Win/Linux) and is at v2.2 ship-readiness, but
there is **no way for the developer to learn what is going wrong on users' machines**. The
plugin today has:

- **no logging** (only `DBG`/`jassert`, both stripped in release),
- **no networking** of any kind,
- **no crash/error telemetry**.

We want a **privacy-light bug-reporting pipeline**: problems are written to a durable
on-disk queue at the moment they occur (user-initiated *or* auto-detected), and **sent on the
next launch** via background HTTP POST. Deferring delivery to next launch is deliberate — at
bug time the host/plugin may be unstable, so a crash-safe disk queue + background send on
relaunch is the robust pattern. Reporting is **on by default** — with a one-time first-run
notice and an off switch in Settings, so the user can opt out anytime — and the payload is
intentionally minimal (no audio, no parameters, no PII).

## Locked decisions

| Decision | Choice |
|---|---|
| **Triggers** | (a) user **"Report a Bug"** button + (b) **auto anomaly capture** |
| **Delivery** | **HTTP POST** to a single configurable endpoint; queue-to-disk, **drain on next launch** |
| **Consent** | **On by default (opt-out)** for background/auto reports: first-run notice + Settings off switch; explicit Send = informed consent for manual reports |
| **Data scope** | **Minimal**: version, OS, host, sample rate/block size, anomaly summary, optional user text |
| **Format** | **JSON** (`juce::JSON` / `juce::var`) |
| **Install ID** | Anonymous `juce::Uuid`, generated once, stored locally; no PII |
| **Immediate flush** | After a user submit, attempt one background send immediately; fall back to next-launch retry |
| **Ownership** | Subsystem owned by **`PluginProcessor`** (works headless, true "launch" = instantiation) |

### Auto-capture reality check
`RTAllocationGuard` (`Source/RTAllocationGuard.h`) and `jassert` are **debug/test-only**
(`DISTORTION_RT_GUARD` is set on the test target only; asserts are stripped in release). They
are **not** a runtime signal in shipping builds. The realistic always-on anomaly signal in a
release plugin is **non-finite audio detection** — a cheap `std::isfinite` probe in
`processBlock` feeding an atomic counter. Richer context (stack traces) is not RT-safe, so
auto reports carry **counters + minimal context, not traces**.

### Explicitly out of scope (v1)
- **Crash handler** (`SystemStats::setApplicationCrashHandler`) — deselected; hard segfaults
  won't be captured. Can be added later as an isolated increment.
- Parameter/preset state, audio capture, session recording — excluded for privacy.

---

## Architecture

Five small, independently-testable units. **The on-disk queue is the source of truth**;
everything else produces into it or drains from it.

```
 [AUDIO THREAD]              [MESSAGE THREAD]                  [DISK]                    [NETWORK]
 non-finite probe
   │ atomic++                                                                
   ▼                                                                         
 DiagnosticsSink ──drain──►  ReportComposer ──serialize──►  ReportStore       
 (lock-free counters)            ▲                          (reports/*.json)   
                                 │                              │  at launch    
 "Report a Bug" dialog ─text────┘                              │  (+ after submit)
                                                               ▼               
 consent: atomic<bool> on processor ── gate ──►         ReportSender ──POST──► endpoint URL
   (init from settings.xml, set by editor)        drain: deferred ~4s, once/proc, IPLock (M1/M2)
   installId: own atomic file                     bg thread, timeouts; 2xx→delete else keep≤cap
```

### Components & contracts

- **`DiagnosticsSink`** *(new — `Source/Diagnostics/DiagnosticsSink.h`)*
  Lock-free anomaly counters, e.g. `std::atomic<uint32_t> nonFiniteBlocks`,
  `std::atomic<uint32_t> processBlockCount`. Audio thread **only increments**; message thread
  reads + resets to build a summary. RT-safe by construction (no alloc, no lock).
  *Depends on:* nothing.

- **`ReportStore`** *(new — `Source/Diagnostics/ReportStore.{h,cpp}`)*
  Owns `…/<appData>/<product>/reports/`, a sibling of the existing `settings.xml` /
  preset dir (`PluginEditor.cpp:1663`, `getPresetDirectory()`). API:
  `File enqueue(const Report&)`, `Array<File> listPending()`, `File claim(const File&)`
  (atomic rename `*.json` → `*.sending` before POST — M1), `void remove(const File&)`,
  `void purge(predicate)`, `void prune()`. **Queue hygiene (M3):** `enqueue`/`prune` enforce a
  hard cap (≤ ~50 files, drop oldest) and max age (~30 days); stale `*.sending` claims left by a
  crashed drainer are reverted to `*.json` on startup. Durable across crash/quit.
  *Depends on:* `juce::File`.

- **`ReportComposer`** *(new — `Source/Diagnostics/ReportComposer.{h,cpp}`)*
  Builds a `Report` from `GIT_VERSION_STRING` (`Source/GitVersion.h`), `SystemStats` (OS),
  `wrapperType` + `juce::PluginHostType` (host), sample rate + block size (captured in
  `prepareToPlay`), the drained `DiagnosticsSink` summary, `installId`, a `trigger` tag
  (`"user"` | `"auto"`), and optional user text. Serializes to JSON.
  *Depends on:* sink + version + processor snapshot.

- **`ReportSender`** *(new — `Source/Diagnostics/ReportSender.{h,cpp}`)*
  On launch (and after a user submit) drains the queue on a **cooperatively-stoppable
  background thread** (`juce::Thread`); never on the message or audio thread. Per file: `claim`
  it, POST via `ITransport`; 2xx → `remove`; non-2xx/error → revert claim, keep for retry.
  **Single-drainer (M1):** the launch drain runs only while holding
  `juce::InterProcessLock("MonolitDistortionReportDrain")` (cross-process — covers concurrent
  instances *and* Standalone+VST3 at once); contenders skip. **Scan-safe (M2):** the launch
  drain is kicked by a **deferred ~4 s `Timer`** + once-per-process guard, so host/`pluginval`
  construct→destroy scan storms tear down before it fires. **Bounded (M3/M4):** ≤ ~10 sends per
  launch; every request uses explicit connect/read **timeouts**; the thread stops cleanly so
  teardown never blocks on a pending POST. Backend access goes through a tiny **`ITransport`**
  interface so tests inject a stub (no network in CI). *Depends on:* store + transport +
  consent gate.

- **Consent + UI** *(edits to `PluginEditor` / `SettingsState` / `PluginProcessor`)*
  Add `bool bugReportsEnabled = true;` to `SettingsState` (`PluginEditor.h:654`) — one
  `setAttribute`/`getBoolAttribute` pair, mirroring existing fields (persistence only,
  editor-written). **The live gate (C1) is a `std::atomic<bool>` on `PluginProcessor`**,
  initialized from `settings.xml` at construction and updated *directly* by the editor toggle —
  the file is never re-read to decide a send. The anonymous **`installId`** lives in its **own
  atomically-written file** (`…/<product>/installId`, temp-write + rename), **not** in the
  non-atomic `settings.xml` (`PluginEditor.h:672`) — so it adds no new multi-instance writer to
  that file. Add a **consent toggle** ("Send anonymous bug reports", **default ON**), a
  **one-time first-run notice** that explains it and offers an easy opt-out, and a
  **"Report a Bug"** button + small text dialog to the existing Settings overlay /
  `SettingsContent`.

### Shared path helper
`getSettingsFile()` and `getPresetDirectory()` currently live in `PluginEditor`. Factor the
app-data path (`…/<product>/`) into a small shared helper (e.g.
`Source/Diagnostics/AppPaths.h` free functions, or a static on `PluginProcessor`) so the
**processor reads the same `settings.xml`** (to init the consent atomic) and the `installId`
file at construction, without depending on the editor. Reads tolerate a torn/missing file
(absent/torn file → consent defaults **ON**, the product default; a brand-new install also gets
the first-run notice). Keep the change minimal — no behavior change to existing settings.

---

## Concurrency, lifecycle & queue hygiene

The queue is shared across **all instances and processes** (10+-instance projects, Standalone +
VST3 at once — the SoakHarness gates this). The drain path is built around that:

- **M1 — single drainer.** Launch drain runs only while holding
  `juce::InterProcessLock("MonolitDistortionReportDrain")`; other instances skip. Each file is
  **claimed** by atomic rename (`*.json` → `*.sending`) before POST, so no two drainers send the
  same report and a crashed drainer's claims are recoverable (reverted on next startup).
- **M2 — scan-safe trigger.** The launch drain fires from a **deferred ~4 s `Timer`** guarded by
  a once-per-process flag. Host/`pluginval` plugin-scan construct→destroy storms complete in
  milliseconds, so the timer is destroyed before firing — the drain only runs in a real session.
  (The user-submit flush is separate and immediate.)
- **M3 — bounded queue.** `enqueue`/`prune` cap the queue at ≤ ~50 files (drop oldest) and
  ~30 days; each launch sends at most ~10. A permanently-unreachable endpoint can therefore
  never grow the queue without bound or stall startup.
- **M4 — non-blocking, stoppable network.** All POSTs run on a cooperatively-stoppable
  `juce::Thread` with explicit connect/read timeouts; processor teardown signals stop and does
  **not** join-block on an in-flight request.

## Consent & data model (the nuanced part)

**On by default (opt-out)** for background reporting — a one-time first-run notice plus a Settings
off switch — with informed per-action consent for manual reports.
**Invariant: the queue only ever contains consented reports, and consent is re-validated at
send time** — against the processor's live `std::atomic<bool>` gate (C1), never by re-reading
the file.

- **Auto report** (non-finite anomalies): on processor **teardown**, if the consent atomic is
  ON and anomalies are pending, compose + `enqueue` an `auto` report (flushes the session to
  disk). If OFF, anomalies stay in-memory only and are discarded.
- **User report**: the "Report a Bug" button is always available. The Send dialog shows
  **exactly** what will be transmitted; clicking Send = informed consent → `enqueue` a `user`
  report (regardless of the toggle).
- **At drain (next launch)** the gate is re-checked against the consent atomic:
  - `user` reports → **always sent** (consent given at submit time);
  - `auto` reports → sent only if consent is **still** ON; otherwise **purged unsent** (handles
    revocation between sessions).
- **No cancel-after-submit (C2):** a queued `user` report submitted offline will still send on
  the next launch even if the user later changes their mind — consent is point-in-time at the
  moment of Send (like sending an email). Documented behavior, surfaced in the dialog copy.
- **Legal posture (opt-out):** default-on is defensible **because** of the first-run notice +
  always-available off switch + minimal, non-identifying payload. The notice is what makes "on
  by default" honest rather than covert — do **not** ship default-on without it.

### Report payload (JSON)
```json
{
  "schema": 1,
  "trigger": "user",                       // "user" | "auto"
  "installId": "f1e2…",                    // anonymous juce::Uuid
  "pluginVersion": "v2.2.x-<sha>",         // GIT_VERSION_STRING
  "os": "Linux 6.x / Windows 11",          // SystemStats
  "host": { "wrapper": "VST3", "name": "Reaper 7.x" },
  "audio": { "sampleRate": 48000, "blockSize": 512 },
  "anomalies": { "nonFiniteBlocks": 3, "totalBlocks": 120345 },
  "message": "<free text, user reports only>",
  "createdUtc": "2026-06-17T12:00:00Z"
}
```
No parameters, no audio, no file paths, no machine/user names. **PII honesty (C3):** every
auto-collected field is non-PII by construction; the **only** PII vector is the user's own
free-text `message`, which we cannot sanitize — the privacy note and the dialog must tell users
not to type personal data there.

---

## Backend receiver (deliverable, separate from plugin repo)

Plugin is URL-agnostic, so any HTTPS endpoint that accepts the JSON above works.

- **Recommended:** a minimal serverless function (Cloudflare Workers / Cloud Function free
  tier) that validates `schema`, size-limits the body, and appends to storage or relays to
  email. Returns **2xx** on accept (plugin deletes the queued file on 2xx only).
- **Zero-code fallback:** a form-backend (Formspree/Basin-style) endpoint so reporting can go
  live before custom infra exists; swap the URL later with no plugin change.
- **Contract:** `POST <endpoint>`, `Content-Type: application/json`, body = report JSON.
  Success = HTTP 2xx. Any non-2xx / network error = keep file, retry next launch.
- Endpoint URL is a single compile-time constant (or build-time define), e.g.
  `Diagnostics/ReportEndpoint.h`.

---

## RT-safety notes (per `realtime-audio-safety-checklist.md` + AGENTS.md)

- `DiagnosticsSink` increments are plain relaxed `std::atomic` ops — **no allocation, no lock,
  no syscall** on the audio thread.
- The non-finite probe is a cheap branch in `processBlock`; gate behind the existing bypass
  fast-path so it costs ~nothing when bypassed.
- All file IO and networking happen on the **message thread / background threads** only.
- Launch-time drain is kicked off **deferred** (~4 s `Timer`, once-per-process — M2),
  mirroring the existing RT-safe deferred-rebuild and deferred-`saveSettings` patterns
  (`PluginEditor.cpp:1432/1509`). Never block construction or `processBlock`.

---

## Testing strategy (JUCE UnitTest runner; no network in CI)

- **`ReportStore`**: `enqueue` writes a file; `remove` deletes; pending list survives across
  separate instances (durability); `purge` drops only matching files.
- **`ReportComposer`**: snapshot fields populated; JSON round-trips; `user` vs `auto` tagging.
- **`DiagnosticsSink`**: drain produces correct counts; **assert zero allocations** on the
  increment path using the existing `DISTORTION_RT_GUARD` (`rt_guard::getAllocationCount()`).
- **`ReportSender`**: with a **stub `ITransport`** — 2xx deletes the file; non-2xx keeps it;
  consent OFF sends no `auto` reports and **purges** them; `user` reports send regardless.
- **Consent gate**: nothing is enqueued/sent for `auto` while OFF; flip-OFF-between-sessions
  purges pending `auto` reports.
- Reuse existing categories/structure in `Source/Tests/DistortionTests.cpp`.

---

## Implementation roadmap

Each step is independently testable; land in order.

1. **Core, no UI/audio** — `ITransport` interface + `ReportStore` (enqueue/list/`claim`/remove/
   `prune`, caps + age, stale-claim recovery — M1/M3) + `AppPaths` helper. Tests:
   enqueue/remove/list/durability, cap+age pruning, claim/revert.
2. **`ReportComposer`** — env snapshot + JSON serialize + `trigger`; reads the `installId` file
   (atomic create-if-missing). Round-trip tests.
3. **`DiagnosticsSink`** + non-finite probe in `processBlock`; capture sampleRate/blockSize in
   `prepareToPlay`. RT-safety test via `DISTORTION_RT_GUARD`.
4. **`ReportSender`** — stoppable bg thread + timeouts (M4), `InterProcessLock` single-drainer
   (M1), ≤10/launch (M3), consent re-check/purge. Tests with stub transport, incl. a
   two-drainer no-duplicate-send test.
5. **Consent gate** — `bugReportsEnabled` (**default ON**) in `SettingsState` (persistence) +
   `std::atomic<bool>` on `PluginProcessor` init'd at construction and set by the editor toggle
   (C1); gating. Add the one-time first-run notice + Settings off switch.
6. **"Report a Bug"** button + dialog in the Settings overlay; informed-consent + "sends now or
   next launch" + "don't type personal data" copy (C2/C3).
7. **Wire lifecycle** — deferred ~4 s scan-safe drain timer + once-per-process guard (M2);
   flush `auto` report on teardown; immediate background flush after user submit.
8. **Backend receiver** *(separate)* — minimal serverless endpoint + documented contract; set
   the endpoint constant.
9. **Docs** — privacy note (collected fields, default-on + how to opt out, no-PII-in-free-text), `RELEASE_CHECKLIST.md`
   gate, update `AGENTS.md` (new subsystem + paths).

---

## Risks / tradeoffs

- **You maintain a backend.** Mitigated by URL-agnostic transport + zero-code fallback; can
  swap to email/Sentry later without touching the plugin's queue/log layer.
- **No crash handler** → hard crashes (segfaults) are not captured; only detectable anomalies
  + user reports. Accepted for v1.
- **Send-on-next-launch ≠ instant** for offline user reports (mitigated by the post-submit
  flush attempt); **and not cancellable once submitted** (C2).
- **Shallow auto-capture** — counters, not traces (RT-safety constraint).
- **Headless coverage** — anomalies during UI-less processing are still captured because the
  subsystem lives in the processor and flushes at teardown.
- **Multi-instance/multi-process is handled, not assumed away** — the shared queue is safe under
  10+ instances and Standalone+VST3 via the single-drainer lock + atomic claim (M1) and the
  scan-safe deferred trigger (M2); the queue is bounded so a dead endpoint can't grow it without
  limit (M3).
