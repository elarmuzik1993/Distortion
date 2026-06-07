# Real-Time Audio Safety & Correctness Checklist

Use the left column to mark ✓/✗. If an item fails, follow the "Fix" note.

---

## 1. Audio Callback Hygiene

| ✓/✗ | Check |
|-----|-------|
| [ ] | **No heap allocations in `processBlock()`** — no `new`, `make_unique`, `std::string` construction, etc. |
|     | *Fix: Move allocations to `prepareToPlay()`; pre-construct and reuse objects.* |
| [ ] | **No STL containers changing capacity** — no `push_back`, `emplace_back`, `resize` that grows. |
|     | *Fix: `vector.reserve()` in `prepareToPlay()` and use indexed writes.* |
| [ ] | **No file I/O, logging, console output, or GUI calls** in audio thread. |
|     | *Fix: Send events to background thread or set atomic flag for main thread.* |
| [ ] | **No blocking synchronization** — no `std::mutex`, `std::lock_guard`, condition variables. |
|     | *Fix: Use lock-free structures, atomics, or `juce::AbstractFifo`.* |
| [ ] | **No exceptions thrown or escaping** from audio thread. |
|     | *Fix: Catch/handle exceptions; avoid throwing on audio path entirely.* |

---

## 2. Threading & Background Work

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Heavy work runs on worker threads** — model loading, file I/O, training, etc. |
|     | *Fix: Move to worker thread; marshal results via atomics or lock-free FIFO.* |
| [ ] | **Thread communication uses atomics or lock-free queues** — no locks around audio. |
|     | *Fix: `std::atomic` for flags; `juce::AbstractFifo` or ring buffer for data.* |
| [ ] | **Background results applied safely** — no mid-sample abrupt swaps. |
|     | *Fix: Swap pointers/indices atomically; smoothly ramp parameters.* |
| [ ] | **No system-message-queue posts from the audio thread** — `triggerAsyncUpdate()` / `postMessage()` / `MessageManager::callAsync()` are thread-safe but NOT realtime-safe (on Linux they take a lock and `write()` the wake pipe). APVTS `parameterChanged()` can run on the audio thread (host automation/preset load). |
|     | *Fix: From the audio thread, set a wait-free `std::atomic` flag only; drain it on the message thread with a `juce::Timer` and do the deferred work there. See snippet below.* |

---

## 3. Parameter Handling & Smoothing

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Parameter changes are smoothed** — no raw values causing zipper noise. |
|     | *Fix: Use `juce::SmoothedValue` or per-sample one-pole smoothing.* |
| [ ] | **Host automation handled per-sample** or properly interpolated per-block. |
|     | *Fix: Read parameter buffer per-sample or generate interpolated ramp.* |
| [ ] | **Preset/program changes update DSP safely** — use atomic flags to trigger reset. |
|     | *Fix: Set atomic; handle reset at top of next `processBlock()`.* |

---

## 4. Prepare / Reset / Sample-Rate Handling

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Sample-rate dependent coefficients recalculated** in `prepareToPlay()` and on SR change. |
|     | *Fix: Provide `prepare(sampleRate, samplesPerBlock)` in DSP modules.* |
| [ ] | **Buffers sized to `samplesPerBlock`** allocated ahead of time, not per-block. |
|     | *Fix: Allocate once in `prepareToPlay()` and reuse.* |
| [ ] | **`reset()` clears filter states / delay lines** when needed. |
|     | *Fix: Implement `reset()`; call on bypass toggles, channel config changes.* |
| [ ] | **Plugin responds to host SR/buffer changes** — verify `prepareToPlay()` recalculates everything. |
|     | *Fix: Test by changing buffer size mid-session in DAW.* |

---

## 5. Bypass Handling

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Bypass doesn't introduce clicks** — use crossfade or `processBlockBypassed()`. |
|     | *Fix: Implement short crossfade (5-10ms) when toggling bypass.* |
| [ ] | **Bypassed state maintains filter continuity** — decide if filters should keep running or reset. |
|     | *Fix: Either process silently to keep state, or reset cleanly on un-bypass.* |

---

## 6. Channel Configuration

| ✓/✗ | Check |
|-----|-------|
| [ ] | **No hardcoded channel counts** — use `getTotalNumInputChannels()` / `getTotalNumOutputChannels()`. |
|     | *Fix: Loop over actual channel count, not assumed 2.* |
| [ ] | **Mono input to stereo output handled** if supported. |
|     | *Fix: Check `isBusesLayoutSupported()` and handle layouts explicitly.* |
| [ ] | **Surround configurations tested** if plugin claims support. |
|     | *Fix: Test in DAW with surround bus or disable unsupported layouts.* |

---

## 7. Numeric Safety

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Denormals prevented** — use `juce::ScopedNoDenormals` at start of `processBlock()`. |
|     | *Fix: Add `juce::ScopedNoDenormals noDenormals;` as first line.* |
| [ ] | **NaN/Inf cannot propagate** — protect division, feedback, and exponential operations. |
|     | *Fix: Clamp inputs; use `std::isfinite()` checks on debug builds; sanitize feedback.* |
| [ ] | **No uninitialized memory** — all buffers zeroed or written before read. |
|     | *Fix: Zero buffers in `prepareToPlay()` or at start of `processBlock()`.* |
| [ ] | **Randomness is explicit and seeded** — no implicit `rand()` per sample. |
|     | *Fix: Use PRNG with controlled seed if repeatable behavior needed.* |

---

## 8. Performance & CPU Stability

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Inner loops avoid branches and heap work** — simple arithmetic, tables, SIMD-friendly. |
|     | *Fix: Profile hot functions; use lookup tables for expensive math.* |
| [ ] | **No expensive coefficient calculations per-sample** — move to per-block or on param change. |
|     | *Fix: Debounce coefficient updates; only recalc when params actually change.* |
| [ ] | **Buffer aliasing safe** — not reading/writing same location unsafely in-place. |
|     | *Fix: Use separate read/write pointers or copy to temp buffer first.* |

---

## 9. GUI ↔ Audio Separation

| ✓/✗ | Check |
|-----|-------|
| [ ] | **GUI doesn't call audio routines directly** — use APVTS or atomics. |
|     | *Fix: Use `AudioProcessorValueTreeState` attachments or atomic variables.* |
| [ ] | **No heavy GUI work triggered from audio thread** — images, layout, etc. |
|     | *Fix: Enqueue GUI updates to message thread via `AsyncUpdater`.* |

---

## 10. Host Integration & Latency

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Latency reported correctly** — `setLatencySamples()` for any look-ahead or internal buffering. |
|     | *Fix: Compute and report; host needs this for PDC (plugin delay compensation).* |
| [ ] | **Tail time reported** — `getTailLengthSeconds()` returns actual tail for reverbs/delays/feedback. |
|     | *Fix: Calculate tail from delay times or decay; return in seconds.* |

---

## 11. State Persistence

| ✓/✗ | Check |
|-----|-------|
| [ ] | **`getStateInformation()` / `setStateInformation()` are thread-safe** — don't touch audio state directly. |
|     | *Fix: Copy to/from atomics or use APVTS `copyState()` / `replaceState()`.* |
| [ ] | **State restore doesn't cause glitches** — apply smoothly or at safe point. |
|     | *Fix: Set atomic flag; audio thread picks up new state at block boundary.* |

---

## 12. Testing & Validation

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Test multiple buffer sizes** — 32, 64, 128, 256, 512, 1024, 2048. |
|     | *Watch for: xruns, glitches, crashes at small buffers.* |
| [ ] | **Test multiple sample rates** — 44.1k, 48k, 88.2k, 96k, 176.4k, 192k. |
|     | *Watch for: wrong pitch, broken filters, aliasing.* |
| [ ] | **Test multiple DAWs** — Ableton, FL Studio, Reaper, Logic, Pro Tools, Bitwig. |
|     | *Watch for: GUI issues, parameter automation differences, bypass behavior.* |
| [ ] | **CPU stress test** — run many instances, watch for xruns. |
|     | *Tools: DAW CPU meter, instruments like PluginDoctor.* |
| [ ] | **Auditory sweep test** — quickly sweep Drive/Level/Presence, listen for zipper/clicks. |
|     | *Fix: Per-sample smoothing and anti-click crossfades.* |
| [ ] | **Automated DSP unit tests** — verify filter responses, waveshaper curves, expected harmonics. |
|     | *Fix: Write small tests; catch regressions early.* |

---

## 13. Third-Party & Code Hygiene

| ✓/✗ | Check |
|-----|-------|
| [ ] | **Third-party libs in audio path are RT-safe** — review their source. |
|     | *Fix: Avoid non-RT-safe libs in `processBlock()`.* |
| [ ] | **Dev artifacts in `.gitignore`** — `*.filtergraph`, build folders, IDE caches. |
|     | *Fix: Commit only source, project files, and intentional build artifacts.* |

---

## Quick Code Snippets

### Scoped Denormals
```cpp
void MyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // ... process here
}
```

### SmoothedValue Usage
```cpp
// In class declaration
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;

// In prepareToPlay()
void prepareToPlay(double sampleRate, int /*samplesPerBlock*/) override
{
    driveSmoothed.reset(sampleRate, 0.02); // 20ms smoothing time
}

// In processBlock() - update target from parameter
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
{
    driveSmoothed.setTargetValue(*driveParameter);
    
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float drive = driveSmoothed.getNextValue();
        // use 'drive' per-sample
    }
}
```

### Pre-allocate Vector (Correct Way)
```cpp
// Option A: Fixed size, use indexing
std::vector<float> tempBuffer;

void prepareToPlay(double, int samplesPerBlock) override
{
    tempBuffer.resize(samplesPerBlock, 0.0f);  // Size is fixed, write by index
}

// Option B: Dynamic use with reserve
std::vector<float> tempBuffer;

void prepareToPlay(double, int samplesPerBlock) override
{
    tempBuffer.clear();
    tempBuffer.reserve(samplesPerBlock);  // Capacity set, use push_back safely
}
```

### NaN/Inf Protection
```cpp
inline float sanitize(float x)
{
    // Clamp to safe range, catches NaN and Inf
    if (std::isnan(x) || std::isinf(x))
        return 0.0f;
    return std::clamp(x, -10.0f, 10.0f);  // Adjust range as needed
}

// Use in feedback paths
feedback = sanitize(delayLine.read()) * feedbackAmount;
```

### Bypass Crossfade
```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    
    bool bypassed = *bypassParameter > 0.5f;
    
    if (bypassed && !wasBypassed)
    {
        // Transitioning to bypass - fade out wet
        bypassRamp.setTargetValue(0.0f);
    }
    else if (!bypassed && wasBypassed)
    {
        // Transitioning from bypass - fade in wet
        bypassRamp.setTargetValue(1.0f);
    }
    wasBypassed = bypassed;
    
    // Process DSP into wet buffer
    wetBuffer.makeCopyOf(buffer);
    processDSP(wetBuffer);
    
    // Mix dry/wet based on bypass ramp
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dry = buffer.getWritePointer(ch);
        auto* wet = wetBuffer.getReadPointer(ch);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float mix = bypassRamp.getNextValue();
            dry[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
        }
    }
}
```

### Deferring work off the audio thread (atomic flag + Timer)
```cpp
// parameterChanged() may be called on the AUDIO thread by host automation /
// preset load. Do NOT triggerAsyncUpdate() here — posting to the system message
// queue is not realtime-safe. Flip a wait-free flag instead.
class MyProcessor : public juce::AudioProcessor,
                    private juce::Timer,
                    private juce::AudioProcessorValueTreeState::Listener
{
    std::atomic<bool> rebuildPending{ false };

    MyProcessor() { startTimer(50); }   // message-thread poll; auto-stops on destruct
    ~MyProcessor() override { stopTimer(); }

    void parameterChanged(const juce::String& id, float) override
    {
        if (id == "heavyParam")
            rebuildPending.store(true, std::memory_order_release);  // wait-free
    }

    void timerCallback() override
    {
        // exchange() coalesces multiple requests into one rebuild per tick.
        if (rebuildPending.exchange(false, std::memory_order_acq_rel))
            doHeavyRebuild();   // allocating work, safe on the message thread
    }
};
```
*Note: the `juce::Timer` only fires when a message loop is running (plugin host /
standalone), so headless unit tests are unaffected — drive the rebuild directly there.*

---

## How to Use This Checklist

1. Open this file next to your code
2. Go item-by-item; mark ✓ or ✗
3. If ✗, apply the fix and re-test immediately
4. When all ✓, run multi-DAW and multi-buffer stress tests
5. Re-run this checklist after any significant DSP changes

---

*Last updated: June 2026*
