// Renders headline options for the launch post and a side-by-side sheet:
//   node marketing/launch/variants.js
// Output: marketing/launch/variants/<n>.png (1080x1350) and variants/sheet.png
const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

const VARIANTS = [
  { k: '808 · BASS · GUITAR · NAM',        l1: 'Distort the 808.',  l2: 'Keep the sub.' },
  { k: 'FREE DISTORTION PLUGIN',           l1: 'Hit it with',       l2: 'the Sledge.' },
  { k: 'SUB GUARD · 7 CLIP TYPES · NAM',   l1: 'All the grit.',     l2: 'None of the mud.' },
  { k: 'DRAW THE EQ ON THE WAVEFORM',      l1: 'Shape your dirt',   l2: 'by hand.' },
  { k: 'NO ILOK · NO ACCOUNT · NO PRICE',  l1: 'Heavy. Free.',      l2: 'Open source.' },
  { k: 'CLIP TYPES · NAM CAPTURES · EQ',   l1: 'Break the signal.', l2: 'Not the mix.' },
];

(async () => {
  const dir = path.join(__dirname, 'variants');
  fs.mkdirSync(dir, { recursive: true });
  const page = 'file://' + path.join(__dirname, 'post.html');
  const browser = await chromium.launch();
  const p = await browser.newPage({ viewport: { width: 1080, height: 1350 }, deviceScaleFactor: 1 });
  const files = [];
  for (const [i, v] of VARIANTS.entries()) {
    await p.goto(page + '?' + new URLSearchParams(v).toString());
    await p.evaluate(() => document.fonts.ready);
    const f = path.join(dir, `${i + 1}.png`);
    await p.screenshot({ path: f });
    files.push(f);
  }

  // Contact sheet: 3 x 2, each post at 40% with its number underneath.
  const cells = files.map((f, i) =>
    `<figure><img src="file://${f}"><figcaption>${i + 1}</figcaption></figure>`).join('');
  await p.setViewportSize({ width: 3 * 432 + 4 * 24, height: 2 * (540 + 56) + 24 });
  await p.setContent(`<style>
      body { margin: 0; background: #1a1a1f; display: grid; grid-template-columns: repeat(3, 432px);
             gap: 24px; padding: 24px; font: 700 30px Inter, sans-serif; color: #fff; }
      figure { margin: 0; text-align: center; } img { width: 432px; height: 540px; display: block; }
      figcaption { margin-top: 8px; }
    </style>${cells}`);
  await p.waitForLoadState('load');
  await p.screenshot({ path: path.join(dir, 'sheet.png') });
  await browser.close();
})();
