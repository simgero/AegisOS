// Run with NODE_PATH pointing to the installed Playwright dependency.
// Only renders the code-native layout; approved source artwork stays unchanged.
const { chromium } = require('playwright');
const fs = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
(async () => {
  const output = path.resolve(__dirname, '../../out/branding/part0');
  await fs.mkdir(output, { recursive: true });
  const browser = await chromium.launch({ headless: true, channel: 'chrome' });
  try {
    const page = await browser.newPage({ viewport: { width: 720, height: 720 }, deviceScaleFactor: 1 });
    await page.goto(pathToFileURL(path.join(__dirname, 'boot.html')).href);
    await page.evaluate(async () => {
      await document.fonts.ready;
      await Promise.all([...document.images].map(image => image.decode()));
      if (!document.fonts.check('700 58px Arial')) throw new Error('Arial required');
    });
    for (let n = 0; n < 36; n++) {
      await page.evaluate(n => window.frame(n), n);
      await page.screenshot({ path: path.join(output, `${String(n).padStart(3, '0')}.png`) });
    }
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
