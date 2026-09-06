## Summary of Changes

<!-- A concise description of what this PR introduces or fixes -->

## Real-Time Audio & Architecture Invariants

- [ ] **No heap allocations in audio thread**: No `new`/`delete`, `std::vector` resize, `juce::String` creation, or memory operations inside `processBlock` or sub-stage helpers.
- [ ] **No locks in fast path**: Scope queue and visualization use lock-free SPSC structures (`ScopeBuffer`).
- [ ] **Phase coherence preserved**: Dry/wet alignment maintained across DSP changes.
- [ ] **Bypass compensation**: PDC and latency reporting updated if DSP latency changed.

## Testing & Verification

- [ ] `cmake --build build --target DistortionTests` runs with zero failures.
- [ ] Added new unit test assertions for new functionality or bug fixes.
- [ ] Verified host compatibility or `pluginval` strictness level 10 pass.
