# State

## Now
- `feat/nam-profile-prototype` (no PR): NAM profile prototype; a loaded `.nam` replaces the clip
  type. Local is 2 commits ahead of origin (unpushed): `95c5fda` links `nam_core` objects into
  each plugin format (profiles failed to load in Standalone/VST3/AU: "No config parser
  registered"), `b2700c8` opt-in ASIO for the Windows standalone.
- Windows/MSVC confirmed on 2026-09-26: `build-rel` Release builds Standalone + VST3 + tests;
  `DistortionTests` passes 3175 assertions; `DistortionRender --profile` renders the owner's
  SlimmableContainer profile `SD1 T0 D10.nam`. Loading it through the Standalone UI: unverified.
- `fix/filter-mode-switch-cutoff` (`ff0ad79`, off `origin/main` `f516f93`, unpushed), in the
  worktree `~/Distortion-filterfix`: switching filter mode in the UI moves a cutoff still at the
  old mode's default to the new one's (HP 20 Hz, LP 20 kHz, BP 1 kHz). 3138 assertions pass;
  not tried by hand in the UI.
- PR #54 (`chore/housekeeping-debug`) open and unmerged; its head carries `[skip ci]`.

## Next
1. Owner: in the ASIO standalone (`build-asio/Distortion_artefacts/Release/Standalone/`), pick
   ASIO at 48 kHz, PROFILE -> Load profile... `~/Desktop/MONOLIT BEATZ/Tones/Boss SD-1/SD1 T0
   D10.nam`, sweep Distortion Amount with Sub Guard on; then the VST3 in a DAW, save + reopen.
2. Owner: click through the filter modes in the `~/Distortion-filterfix` build; if happy, push
   the fix branch and open a PR.
3. Push `feat/nam-profile-prototype` when asked; dispatch "Build VST3 Plugin" on it to confirm
   macOS (AppleClang) and Linux with the linking fix (untested there).
4. Fix issue #44 on its own branch (lead under Known issues).
5. Prototype gaps: resampling for non-48 kHz hosts, crossfade on profile swap, fixed latency
   across profile on/off, EXTREME with profiles, profile in Sledge's presets, choosing a
   container's smaller submodels. Longer plan: https://claude.ai/code/artifact/e68e2d2d-28d9-48fe-ace0-ee47ca6b7bcb
6. Owner decision: 41 commits on `origin/main` are authored `Claude <noreply@anthropic.com>`
   (latest 2026-09-07), against the no-AI-attribution rule. Fixing needs a history rewrite.

## Decisions
- Chain runs at 1x while a profile is loaded (no resampling yet); off-48 kHz hosts get an amber button.
- Sessions remember a profile by file path, not embedded.
- NAM Core pinned `0b3d3c9`, OBJECT library at C++20, and its objects also go straight into each
  format target: the shared-code static lib would drop the self-registering architectures.
- ASIO is opt-in (`-DDISTORTION_ASIO_SDK_DIR=<SDK>/ASIOSDK`) in a separate `build-asio` tree, so
  `build-rel` stays standard. Sledge is GPLv3 since `fbb94eb`, so the SDK's GPLv3 option applies.
- Filter-mode rule fires only on a user gesture; presets, sessions, randomize, automation untouched.
- Distortion Amount -> model input gain: 0% -24 dB, 50% -6 dB, 100% +12 dB. NAM fast tanh on.

## Known issues
- No `scripts/verify.sh`. Verify = MSVC via vcvars64 (toolchain off PATH), build Standalone +
  `DistortionTests`, run the tests. The NAM tests link `nam_core` directly, so they cannot catch
  the format-linking bug; check the plugin binary contains "SlimmableContainer".
- Close `Sledge Distortion.exe` before rebuilding (LNK1104).
- On `main`, a build rewrites tracked `Source/GitVersion.h`; `git checkout` it (PR #54 fixes).
- #44 pluginval segfault (Automation, 96 kHz / 64), intermittent. Lead: VST3 wrapper calls
  `prepareToPlay` without the callback lock while the 50 ms timer can `rebuildOversampling`.
- Low Pass at a low cutoff mutes the distortion by design: the filter runs before the drive.
- `.agents/` is committed on this branch while other AI files are untracked; decide before merging.
