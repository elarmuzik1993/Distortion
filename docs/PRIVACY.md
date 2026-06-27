# Monolit Distortion — Privacy & Bug Reporting

Monolit Distortion can send **anonymous bug reports** to help us find and fix
problems. This page explains exactly what is (and isn't) collected, and how to
turn it off.

## In short

- **On by default, opt-out.** The first time you open the plugin you'll see a
  one-time notice. You can turn reporting off at any time in **Settings → "Send
  anonymous bug reports"**.
- **Minimal, anonymous data.** No audio, no parameter/preset values, no file
  paths, no account, no machine or user names.
- **Nothing is sent the moment a problem happens.** Reports are written to a
  small queue on disk and sent quietly **on the next launch**, in the background
  — so a crash never loses the report and reporting never interferes with audio.

## Two ways a report is created

1. **Automatic** — if the plugin detects non-finite audio (a NaN/Inf "blow-up")
   while reporting is enabled, it records a small anonymous summary.
2. **Manual** — the **"Report a Bug"** button in Settings lets you describe a
   problem in your own words. Clicking **Send** is your consent for that report.

## What a report contains

| Field | Example | Why |
|---|---|---|
| Plugin version | `v2.2.x-<sha>` | Know which build is affected |
| Operating system | `Linux 6.x` / `Windows 11` | Reproduce the environment |
| DAW host | format (`VST3`/`Standalone`) + name (e.g. `Reaper`) | Host-specific bugs |
| Audio setup | sample rate + block size | Reproduce the audio config |
| Anomaly summary | counts of non-finite vs total blocks | Detect blow-ups |
| Anonymous install ID | random UUID generated on this install | De-duplicate reports from one install — **not** tied to your identity |
| Trigger + timestamp | `auto`/`user`, UTC time | Triage |
| Your message | *(manual reports only)* | What you typed in "Report a Bug" |

### The one thing to be careful about
The **free-text message** in a manual report is the only field you control, and
we can't filter it. **Please don't type personal information** (name, email,
license keys, file paths) into it.

## What is **not** collected
No audio is ever captured. No parameter values, preset contents, project data,
file paths, machine names, user names, or persistent identifiers beyond the
random per-install UUID above.

## How it's sent
Reports are queued as small JSON files under your user application-data folder
(e.g. `~/.config/MonolitBeats/Monolit Distortion/reports/` on Linux; the
equivalent AppData/Application Support folder on Windows/macOS) and POSTed over
HTTPS to an endpoint operated by Monolit Beats on the next launch. The queue is
**bounded** (a small cap on count and age), so it can never grow without limit —
even if the network is unavailable for a long time. If `libcurl` isn't present
on Linux, reporting simply does nothing (it never blocks the plugin from
loading).

## Your control
- Turn it off anytime in **Settings** — automatic reports stop immediately, and
  any still-queued automatic reports are discarded instead of sent.
- Manual reports are only ever sent when you click **Send**.
- To clear the local queue, delete the `reports/` folder shown above.

*Questions: contact Monolit Beats (elar.muzik@gmail.com).*
