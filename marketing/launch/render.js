// Renders post.html to the launch PNGs: node marketing/launch/render.js
const { chromium } = require('playwright');
const path = require('path');

(async () => {
  const page = path.join(__dirname, 'post.html');
  const browser = await chromium.launch();
  const formats = [
    { name: 'sledge-launch-feed.png', hash: '', w: 1080, h: 1350 },
    { name: 'sledge-launch-story.png', hash: '#story', w: 1080, h: 1920 },
  ];
  for (const f of formats) {
    const p = await browser.newPage({ viewport: { width: f.w, height: f.h }, deviceScaleFactor: 1 });
    await p.goto('file://' + page + f.hash);
    await p.evaluate(() => document.fonts.ready);
    await p.screenshot({ path: path.join(__dirname, f.name) });
    await p.close();
  }
  await browser.close();
})();
