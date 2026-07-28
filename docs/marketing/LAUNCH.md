# Launch Checklist — Sledge Distortion (free, email-gate release)

Work top to bottom. Nothing here costs money. The goal: a free plugin nobody
hears about is still a 0/10, so treat distribution as seriously as the build.

---

## Pre-launch gates (must pass before you promote anything)

- [ ] **Tag a release** (`v*`) and confirm CI attached: Windows `.zip` + `.exe`,
      Linux `.tar.gz`, macOS `.pkg`, and `SHA256SUMS`.
- [ ] **Install-test each OS from the actual release files** using `INSTALL.md`
      verbatim — pretend you're a stranger. Windows ZIP, macOS `xattr`, Linux
      tarball. Fix any step that snags.
- [ ] **Diagnostics smoke test:** trigger one "Report a Bug" from a release build
      and confirm a row lands in the Supabase `reports` table. (Wiring exists;
      verify the round-trip once with 1 install, not 1,000.)
- [ ] **Landing page live** with: hero GIF, email form (one field, instant
      delivery), before/after audio player, direct download fallback, link to
      `INSTALL.md`.
- [ ] **Email flow works:** submit a test address → link arrives → link downloads.
- [ ] **Demo video + vertical cut uploaded** (unlisted first, sanity-check audio).
- [ ] **"Free for commercial use"** stated on the landing page and every listing.

---

## Distribution — day of launch

Post the **demo clip / GIF first**, text second. Each community has its own tone.

- [ ] **Bedroom Producers Blog (BPB)** — submit via their contact/tip form. This
      is the #1 traffic source for free plugins; they cover nearly every decent
      one. Include: one-line pitch, the hook, GIF, download link, screenshots.
- [ ] **KVR Audio** — create a product listing in the database + a "new free
      plugin" forum post. Own the exact name "Sledge Distortion."
- [ ] **Rekkerd.org** — submit to their free-plugin roundup.
- [ ] **r/edmproduction** — use the correct flair (free plugin/release day
      rules — check the wiki first). Lead with the before/after clip.
- [ ] **r/musicproduction**, **r/WeAreTheMusicMakers** — where rules allow.
- [ ] **VI-Control** and relevant genre Discords — share the video, be present in
      replies.
- [ ] **X/Twitter, Instagram Reels, TikTok, YouTube Shorts** — the 10–15s
      vertical cut, caption = the hook + link in bio.
- [ ] **Your own channels** — pin the release, email any existing contacts.

**Golden rules**
- Don't spam-drop links. Show the clip, answer questions, be a human.
- Read each subreddit's self-promo rules first — one violation can nuke the post.
- Same hook everywhere: *"the free distortion you draw by hand."*

---

## First 72 hours — engage the loop

- [ ] **Reply to every comment/question fast.** Early responsiveness = goodwill +
      more upvotes/shares.
- [ ] **Watch the Supabase `reports` table** for crash/anomaly spikes — your free
      QA army is now live. Hotfix + re-tag if something real surfaces.
- [ ] **Screenshot praise** → save as testimonials for the landing page.
- [ ] **Note feature requests / preset ideas** → fuel for the next update (and a
      reason to email the list again).

---

## After launch — compound it (the whole point of email-grab)

- [ ] Add **testimonials** to the landing page (social proof loop).
- [ ] Ship a **small update** (a preset pack, a bug fix) ~2–4 weeks later — email
      the list. This trains them to open your emails and proves you're active.
- [ ] Keep the list warm with occasional, genuinely useful updates.
- [ ] **When there's traction, buy the $99/yr Apple Developer Program** →
      notarize the macOS build → drop the `xattr` step from `INSTALL.md`. First
      reinvestment priority.
- [ ] Start **product #2** — now you launch to a warm list instead of zero. That
      warm list is the return on giving Sledge away.

---

## Success signals to watch (first month)

- Email signups (the real KPI — not downloads).
- BPB / Rekkerd / KVR pickup (reach multiplier).
- Video watch-through on the first 5s (did the hook land?).
- `reports` table quiet = stable build. Noisy = fix fast.
- Reply sentiment + testimonials collected.
