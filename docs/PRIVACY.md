# Sledge Distortion — Privacy & Bug Reporting (Monolit Beatz)

Sledge Distortion can send **anonymous bug reports** and an **anonymous daily
usage ping** to help us find and fix problems and see how the plugin is used.
This page explains exactly what is (and isn't) collected, and how to turn it
off.

## In short

- **On by default, opt-out.** The first time you open the plugin you'll see a
  one-time notice. You can turn both off at any time in **Settings → "Send
  anonymous bug reports"** — one toggle governs both bug reports and the usage
  ping.
- **Minimal, anonymous data.** No audio, no parameter/preset values, no file
  paths, no account, no machine or user names.
- **Nothing is sent the moment a problem happens.** Reports are written to a
  small queue on disk and sent quietly **on the next launch**, in the background
  — so a crash never loses the report and reporting never interferes with audio.
- **The usage ping is sent at most once per install per day**, directly at
  launch (no on-disk queue) — a missed ping (offline launch) is simply skipped,
  not retried mid-session; the next launch tries again.

## Ways a report or ping is created

1. **Automatic bug report** — if the plugin detects non-finite audio (a NaN/Inf
   "blow-up") while reporting is enabled, it records a small anonymous summary.
2. **Manual bug report** — the **"Report a Bug"** button in Settings lets you
   describe a problem in your own words. Clicking **Send** is your consent for
   that report.
3. **Usage ping** — fires automatically once per day per install, purely to
   count active installs/versions/OSes. Contains no message field and nothing
   you type.

## What a report contains

| Field | Example | Why |
|---|---|---|
| Plugin version | `v2.3.x-<sha>` | Know which build is affected |
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

## What the usage ping contains
A strict subset of the report fields above, and nothing else: **plugin
version, operating system, DAW host, anonymous install ID, and a UTC
timestamp.** No audio setup, no anomaly summary, no message field.

## What is **not** collected
No audio is ever captured. No parameter values, preset contents, project data,
file paths, machine names, user names, or persistent identifiers beyond the
random per-install UUID above.

## How it's sent
Reports are queued as small JSON files under your user application-data folder
(e.g. `~/.config/Monolit Beatz/Sledge Distortion/reports/` on Linux; the
equivalent AppData/Application Support folder on Windows/macOS) and POSTed over
HTTPS to an endpoint operated by Monolit Beatz on the next launch. The queue is
**bounded** (a small cap on count and age), so it can never grow without limit —
even if the network is unavailable for a long time. If `libcurl` isn't present
on Linux, reporting simply does nothing (it never blocks the plugin from
loading).

The usage ping has no queue: it's sent (or skipped) once at launch, and only a
single-line date marker is kept on disk (in the same app-data folder) to track
whether today's ping already went out.

## Your control
- Turn it off anytime in **Settings** — automatic reports and the usage ping
  both stop immediately, and any still-queued automatic reports are discarded
  instead of sent.
- Manual reports are only ever sent when you click **Send**.
- To clear the local queue, delete the `reports/` folder shown above.

*Questions: contact Monolit Beatz (aidevblock@gmail.com).*
