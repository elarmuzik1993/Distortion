# Installing Sledge Distortion

Sledge Distortion is a **VST3** plugin on every platform, plus **AU** and a
**Standalone** app on macOS. Install it, then load it on a track inside your DAW.

> **Is it safe? Why does my OS warn me?**
> Sledge is a free indie release and the builds are **not yet code-signed**
> (signing is a paid certificate we'll add as the plugin grows). Unsigned does
> **not** mean unsafe — it just means your OS can't verify the publisher name,
> so it shows a caution prompt. Every release is built in public on GitHub
> Actions from the source in this repo, and each download has a matching
> **SHA256** checksum, so you can confirm the file arrived intact. The steps
> below get you past the prompt in a few seconds.

---

## Windows

**Recommended: the ZIP (no SmartScreen prompt).**

1. Download `SledgeDistortion-<version>-Windows.zip`.
2. Right-click → **Extract All**.
3. Copy `Sledge Distortion.vst3` into your VST3 folder:
   ```
   C:\Program Files\Common Files\VST3\
   ```
   Windows asks for **administrator permission** to write there. If you'd rather
   not grant it, copy the plugin to your own folder instead and add that path to
   your DAW's plugin search paths:
   ```
   %LOCALAPPDATA%\Programs\Common\VST3
   ```
4. **If the plugin doesn't appear, or your DAW says it failed to load**, run
   `vc_redist.x64.exe` from the ZIP and rescan. This installs the Microsoft VC++
   2015–2022 runtime, which the plugin needs and which clean Windows 10 machines
   often lack. It's safe to run even if you already have it.
5. Rescan plugins in your DAW. Done.

> The ZIP avoids the SmartScreen "Windows protected your PC" box, because
> nothing unsigned is *run* — you're copying a folder. Windows still asks for
> administrator permission at step 3 if you install to the shared VST3 folder;
> that's a normal file-permission prompt, not a security warning about Sledge.

**Prefer an installer?** `SledgeDistortion-<version>-Windows.exe` also exists. It
places the plugin automatically **and** installs the VC++ runtime for you — but
because it's unsigned, Windows SmartScreen shows a blue "Windows protected your
PC" box. Click **More info → Run anyway** to proceed. If you're on a fresh
Windows 10 machine, the installer is the more reliable choice.

**System requirements:** 64-bit Windows 10 or later (ARM64 Windows works via x64
emulation).

---

## macOS

The macOS build is **unsigned**, so macOS quarantines it on download. Clear the
quarantine flag once and it loads normally.

1. Open `SledgeDistortion-<version>-macOS.pkg`. Because it's unsigned,
   **right-click the `.pkg` → Open** (double-clicking will be blocked), then
   confirm. Installs to:
   - VST3 → `/Library/Audio/Plug-Ins/VST3`
   - AU → `/Library/Audio/Plug-Ins/Components`
   - Standalone → `/Applications`
2. Open **Terminal** and run these two lines to clear the quarantine flag. They
   need `sudo` — the installer puts the plugins in a system folder owned by
   root, so without it you'll get `Operation not permitted`. Enter your login
   password when prompted:
   ```bash
   sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Sledge Distortion.vst3"
   sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Sledge Distortion.component"
   ```
3. Rescan plugins in your DAW (in Logic, you may need to reset the AU cache /
   revalidate). Done.

> If Logic Pro rejects the AU on first scan, quit Logic, run the `sudo xattr`
> commands above, then reopen — Logic re-validates and it passes.

**Prefer not to use the installer?** `SledgeDistortion-<version>-macOS.zip`
contains the VST3 and AU bundles to copy into the folders listed above by hand.
The same `sudo xattr` step applies. Note the Standalone app ships only in the
`.pkg`.

**System requirements:** macOS 11.0 (Big Sur) or later. Universal binary — runs
natively on both Apple Silicon and Intel.

---

## Linux

1. Download `SledgeDistortion-<version>-Linux.tar.gz`.
2. Unpack it into your VST3 folder:
   ```bash
   mkdir -p ~/.vst3
   tar -xzf SledgeDistortion-*-Linux.tar.gz -C ~/.vst3
   ```
3. Rescan plugins in your DAW. Done.

No signing prompts on Linux.

---

## Verify your download (optional)

Each release ships a `SHA256SUMS` file listing every download. To confirm your
file arrived complete and uncorrupted, hash the one you downloaded and compare
it against the matching line:

- **Windows (PowerShell):** `Get-FileHash <the-file-you-downloaded> -Algorithm SHA256`
- **macOS / Linux:** `shasum -a 256 <the-file-you-downloaded>`

> This catches a truncated or corrupted download. It is not a substitute for
> code signing: `SHA256SUMS` is published on the same release page as the files
> it describes, so anyone who could alter one could alter both. Signing is
> tracked and will replace this.

---

## Formats & hosts

| Format | Platforms | Example hosts |
|--------|-----------|---------------|
| VST3 | Windows, macOS, Linux | Ableton Live, FL Studio, Cubase, Reaper, Bitwig |
| AU | macOS | Logic Pro, GarageBand |
| Standalone | macOS | run without a DAW to try it out |

> The Standalone app currently ships on macOS only, in the `.pkg`. Windows and
> Linux downloads contain the VST3 plugin.

## Uninstalling

Delete `Sledge Distortion.vst3` (and on macOS the `.component`) from the folders
listed above. Windows installer users can also uninstall from
**Settings → Apps**.

---

Trouble? Found a bug? Sledge has a built-in **Report a Bug** button in its
Settings panel (anonymous, opt-out) — or open an issue on the repo.
