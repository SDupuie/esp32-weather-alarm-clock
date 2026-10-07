// Browser export and validation. Uses the bundled Playwright and installed Chrome.
const { chromium } = require('/Users/Scott/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const path = require('path');
const { pathToFileURL } = require('url');
(async () => {
  const browser = await chromium.launch({ executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1200, height: 1000 } });
    const url = pathToFileURL(path.join(__dirname, 'index.html')).href;
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto(url);
    await page.evaluate(() => document.fonts.ready);
    const missing = await page.locator('img').evaluateAll(ims => ims.filter(i => !i.complete || !i.naturalWidth).map(i => i.src));
    if (errors.length || missing.length) throw Error(JSON.stringify({ errors, missing }));
    await page.screenshot({ path: path.join(__dirname, 'gallery.png'), fullPage: true });
    const ids = await page.locator('article[id]').evaluateAll(els => els.map(e => e.id));
    await page.setViewportSize({ width: 800, height: 800 });
    for (const id of ids) {
      await page.goto(url + '?screen=' + id);
      await page.evaluate(() => document.fonts.ready);
      await page.locator('#' + id + ' .viewport').screenshot({ path: path.join(__dirname, 'screens', id + '.png') });
    }
    await page.goto(pathToFileURL(path.join(__dirname, 'house-messages.html')).href);
    await page.locator('.admin-message').waitFor();
    await page.screenshot({ path: path.join(__dirname, 'screens', 'house-messages.png'), fullPage: true });
    console.log(`Exported ${ids.length} round screens and the actual messaging page. No missing images or script errors.`);
  } finally { await browser.close(); }
})();
