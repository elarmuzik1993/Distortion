# Installing Sledge Distortion

Sledge Distortion is a **VST3 / AU** plugin. Install it, then load it on a track
inside your DAW (it also runs as a Standalone app for quick testing).

> **Is it safe? Why does my OS warn me?**
> Sledge is a free indie release and the builds are **not yet code-signed**
> (signing is a paid certificate we'll add as the plugin grows). Unsigned does
> **not** mean unsafe — it just means your OS can't verify the publisher name,
> so it shows a caution prompt. Every release is built in public on GitHub
> Actions from the source in this repo, and each download has a matching
> **SHA256** checksum you can verify. The steps below get you past the prompt in
> a few seconds.

---

## Windows

**Recommended: the ZIP (no security prompt).**

1. Download `SledgeDistortion-Windows.zip`.
2. Right-click → **Extract All**.
3. Copy `Sledge Distortion.vst3` into:
   ```
   C:\Program Files\Common Files\VST3\
   ```
4. Rescan plugins in your DAW. Done.

> Using the ZIP avoids the Windows SmartScreen popup entirely, because you're
> just copying a file — nothing unsigned is *run*.

**If the plugin doesn't load:** you may be missing the Microsoft Visual C++
runtime (needed on clean Windows 10 machines). Install it once, free, from
Microsoft:
<https://aka.ms/vs/17/release/vc_redist.x64.exe>

**Prefer an installer?** `SledgeDistortion-<version>-Windows.exe` also exists. It
places the plugin automatically **and** installs the VC++ runtime for you — but
because it's unsigned, Windows SmartScreen shows a blue "Windows protected your
PC" box. Click **More info → Run anyway** to proceed.

**System requirements:** 64-bit Windows 10 or later (ARM64 Windows works via x64
emulation).

---

## macOS

The macOS build is **unsigned**, so macOS quarantines it on download. Clear the
quarantine flag once and it loads normally.

1. Open `SledgeDistortion-macOS.pkg`. Because it's unsigned, **right-click the
   `.pkg` → Open** (double-clicking will be blocked), then confirm.
   Installs to:
   - VST3 → `/Library/Audio/Plug-Ins/VST3`
   - AU → `/Library/Audio/Plug-Ins/Components`
   - Standalone → `/Applications`
2. Open **Terminal** and run this one line to clear the quarantine flag:
   ```bash
   xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Sledge Distortion.vst3"
   xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Sledge Distortion.component"
   ```
3. Rescan plugins in your DAW (in Logic, you may need to reset the AU cache /
   revalidate). Done.

> If Logic Pro rejects the AU on first scan, quit Logic, run the `xattr` command
> above, then reopen — Logic re-validates and it passes.

**System requirements:** macOS 11.0 (Big Sur) or later. Universal binary — runs
natively on both Apple Silicon and Intel.

---

## Linux

1. Download `SledgeDistortion-Linux.tar.gz`.
2. Unpack it into your VST3 folder:
   ```bash
   mkdir -p ~/.vst3
   tar -xzf SledgeDistortion-Linux.tar.gz -C ~/.vst3
   ```
3. Rescan plugins in your DAW. Done.

No signing prompts on Linux.

---

## Verify your download (optional)

Each release ships a `SHA256SUMS` file. To confirm your download wasn't
corrupted or tampered with:

- **Windows (PowerShell):** `Get-FileHash SledgeDistortion-Windows.zip -Algorithm SHA256`
- **macOS / Linux:** `shasum -a 256 SledgeDistortion-*.{zip,pkg,tar.gz}`

Compare the output against the matching line in `SHA256SUMS`.

---

## Formats & hosts

| Format | Platforms | Example hosts |
|--------|-----------|---------------|
| VST3 | Windows, macOS, Linux | Ableton Live, FL Studio, Cubase, Reaper, Bitwig |
| AU | macOS | Logic Pro, GarageBand |
| Standalone | all | run without a DAW to try it out |

## Uninstalling

Delete `Sledge Distortion.vst3` (and on macOS the `.component`) from the folders
listed above. Windows installer users can also uninstall from
**Settings → Apps**.

---

Trouble? Found a bug? Sledge has a built-in **Report a Bug** button in its
Settings panel (anonymous, opt-out) — or open an issue on the repo.
