// Builds docs/FM1-Electro-Anleitung.pdf from docs/manual/manual.html with the
// preinstalled Chromium. Usage: node tools/make_manual.js
const path = require('path');
const { chromium } = require('playwright');
(async () => {
  const root = path.join(__dirname, '..');
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('file://' + path.join(root, 'docs/manual/manual.html'));
  await page.evaluate(() => document.fonts.ready);
  const out = path.join(root, 'docs/FM1-Electro-Anleitung.pdf');
  await page.pdf({ path: out, format: 'A4', printBackground: true, preferCSSPageSize: true });
  await browser.close();
  console.log(out);
})();
