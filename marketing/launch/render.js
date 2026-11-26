// Renders post.html to the launch images: node marketing/launch/render.js
//   sledge-launch-feed.png / -story.png        1080 px wide, ready to post
//   hires/sledge-launch-feed@3x.png / -story   3240 px wide, maximum resolution
// The plugin screenshot (assets/ui.png) is the editor painted at 3x pixel density
// by DistortionUiSnapshot, so it stays sharp at 3x:
//   build/DistortionUiSnapshot_artefacts/Release/DistortionUiSnapshot \
//     --scale 100 --eq --signal --pixel-scale 3 --out <dir>
// make-pdf.py then packs the 3x images into one PDF.
const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

(async () => {
  const page = path.join(__dirname, 'post.html');
  const hires = path.join(__dirname, 'hires');
  fs.mkdirSync(hires, { recursive: true });
  const browser = await chromium.launch();
  const formats = [
    { name: 'sledge-launch-feed', hash: '', w: 1080, h: 1350 },
    { name: 'sledge-launch-story', hash: '#story', w: 1080, h: 1920 },
  ];
  for (const f of formats) {
    for (const [scale, out] of [[1, path.join(__dirname, f.name + '.png')],
                                [3, path.join(hires, f.name + '@3x.png')]]) {
      const p = await browser.newPage({ viewport: { width: f.w, height: f.h }, deviceScaleFactor: scale });
      await p.goto('file://' + page + f.hash);
      await p.evaluate(() => document.fonts.ready);
      await p.screenshot({ path: out });
      await p.close();
    }
  }
  await browser.close();
})();
