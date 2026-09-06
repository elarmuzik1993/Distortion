# Security Policy

## Supported versions

The latest release is the supported one. Fixes land on `main` and go out in the
next tagged release.

| Version | Supported |
|---|---|
| 2.3.x | ✅ |
| < 2.3 | ❌ |

## Reporting a vulnerability

**Please do not open a public issue for a security problem.** Report it privately
to **aidevblock@gmail.com** and give me a chance to fix it before it is public.

Useful things to include: what you found, how to reproduce it, the plugin version
and format (VST3 / AU / Standalone), your OS, and what an attacker could actually
do with it.

I am one person, not a security team, so I will not promise a response time I
cannot keep — but I will acknowledge your report and tell you what I intend to do
about it. If you would like credit in the release notes, say so and I will add it.

## What is worth reporting

The plugin is an audio processor, so most of its attack surface is small. The
parts most worth a look:

- **The diagnostics reporter.** Sledge can send anonymous bug reports over HTTPS
  (opt-out, off with one switch). It transmits version, OS, host, sample rate and
  block size, anomaly counts and a random install ID — no audio, no presets, no
  file paths, no personal data. What it collects and how to disable it is in
  [`docs/PRIVACY.md`](docs/PRIVACY.md). If you find it sending anything beyond
  what that document describes, that is a bug worth reporting privately.
- **State and preset loading.** The plugin parses XML state supplied by the host
  and preset files from disk. Malformed input should be rejected, not crash or do
  anything worse.
- **The installers.** Windows Inno Setup and the macOS `.pkg` write to shared
  plugin folders.

## What is already known and not a vulnerability

- **Builds are unsigned.** Windows SmartScreen and macOS Gatekeeper will warn.
  This is a known, documented limitation — see [`INSTALL.md`](INSTALL.md) — and
  code signing is wired into CI, waiting on certificates.
- **`SHA256SUMS` is published alongside the files it describes**, so it detects a
  corrupted download, not a compromised release page. That is stated in
  `INSTALL.md` too, and signing is the real fix.
