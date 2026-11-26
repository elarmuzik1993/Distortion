// Renders post.html to the launch images: node marketing/launch/render.js
//   hires/sledge-launch-feed@3x.png / -story@3x.png   3240 px wide, maximum resolution
//   sledge-launch-feed.png / -story.png               1080 px wide, ready to post
// Then: python3 marketing/launch/make-pdf.py packs the 3x images into one PDF.
//
// The plugin screenshot (assets/ui.png) is docs/images/interface-eq-lfo-compression.png
// cropped to the plugin's frame (1200x705, version stamp hidden). For a sharper
// one, the editor can be painted at 3x pixel density instead:
//   build/DistortionUiSnapshot_artefacts/Release/DistortionUiSnapshot \
//     --scale 100 --eq --signal --pixel-scale 3 --marketing --out <dir>
// Chromium rasterizes the tilted (3D) screen below the page's density, which
// softens it, so the page reports the screen's corners, warp.py does the
// perspective at full quality, and the result is laid over the page flat.
// The 1x images are Lanczos reductions of the 3x ones.
const { chromium } = require('playwright');
const { execFileSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const SCALE = 3;
const here = __dirname;
const hires = path.join(here, 'hires');

(async () => {
  fs.mkdirSync(hires, { recursive: true });
  const browser = await chromium.launch();
  const formats = [
    { name: 'sledge-launch-feed', hash: '', w: 1080, h: 1350 },
    { name: 'sledge-launch-story', hash: '#story', w: 1080, h: 1920 },
  ];
  for (const f of formats) {
    const p = await browser.newPage({ viewport: { width: f.w, height: f.h }, deviceScaleFactor: SCALE });
    await p.goto('file://' + path.join(here, 'post.html') + f.hash);
    await p.evaluate(() => document.fonts.ready);

    // Where the screen's padding box corners land, clockwise from top-left.
    const quad = await p.evaluate(() => {
      const screen = document.querySelector('.screen');
      const at = (css) => {
        const m = document.createElement('div');
        m.style.cssText = 'position:absolute;width:0;height:0;' + css;
        screen.appendChild(m);
        const r = m.getBoundingClientRect();
        m.remove();
        return [r.left, r.top];
      };
      return [at('left:0;top:0'), at('right:0;top:0'), at('right:0;bottom:0'), at('left:0;bottom:0')].flat();
    });

    const overlay = path.join(hires, `.${f.name}-screen.png`);
    execFileSync('python3', [path.join(here, 'warp.py'), path.join(here, 'assets', 'ui.png'), overlay,
      String(f.w), String(f.h), String(SCALE), ...quad.map(String)], { stdio: 'inherit' });

    // The overlay goes inside .stage (its own stacking context, thanks to
    // perspective), offset back to the page origin, so the FREE badge at
    // z-index 2 stays on top of it.
    await p.evaluate(([src, w]) => {
      document.querySelector('.screen img').style.visibility = 'hidden';
      const stage = document.querySelector('.stage');
      const r = stage.getBoundingClientRect();
      const img = document.createElement('img');
      img.src = src;
      img.style.cssText = `position:absolute;left:${-r.left}px;top:${-r.top}px;width:${w}px;` +
                          'z-index:1;pointer-events:none';
      stage.appendChild(img);
      return new Promise((ok) => (img.complete ? ok() : (img.onload = ok)));
    }, ['file://' + overlay, f.w]);

    const big = path.join(hires, `${f.name}@${SCALE}x.png`);
    await p.screenshot({ path: big });
    fs.unlinkSync(overlay);
    await p.close();

    execFileSync('python3', ['-c',
      'import sys; from PIL import Image; im = Image.open(sys.argv[1]); ' +
      'im.resize((int(sys.argv[3]), int(sys.argv[4])), Image.LANCZOS).save(sys.argv[2])',
      big, path.join(here, f.name + '.png'), String(f.w), String(f.h)]);
  }
  await browser.close();
})();
