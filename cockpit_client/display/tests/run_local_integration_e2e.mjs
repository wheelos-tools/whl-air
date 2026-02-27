import { createServer } from 'node:http';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { chromium } from 'playwright';

const HOST = '127.0.0.1';
const PORT = Number(process.env.E2E_PORT || 18080);
const PUBLIC_DIR = new URL('../public/', import.meta.url).pathname;
const PAGE_PATH = process.env.E2E_PAGE || '/rtc_local_integration_test.html';
const E2E_QUERY = process.env.E2E_QUERY || '';
const TARGET_URL = `http://${HOST}:${PORT}${PAGE_PATH}${E2E_QUERY ? `?${E2E_QUERY}` : ''}`;
const HEADLESS = process.env.HEADLESS !== 'false';
const TIMEOUT_MS = Number(process.env.E2E_TIMEOUT_MS || 90000);
const ARTIFACT_DIR = new URL('./artifacts/', import.meta.url).pathname;

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.ico': 'image/x-icon',
};

function safePublicPath(urlPath) {
  const rawPath = decodeURIComponent(urlPath.split('?')[0]);
  const requested = rawPath === '/' ? PAGE_PATH : rawPath;
  const resolved = normalize(join(PUBLIC_DIR, requested));
  if (!resolved.startsWith(normalize(PUBLIC_DIR))) {
    return null;
  }
  return resolved;
}

function createStaticServer() {
  return createServer(async (req, res) => {
    try {
      const filePath = safePublicPath(req.url || '/');
      if (!filePath) {
        res.writeHead(403).end('Forbidden');
        return;
      }

      const data = await readFile(filePath);
      const contentType = MIME[extname(filePath)] || 'application/octet-stream';
      res.writeHead(200, { 'Content-Type': contentType, 'Cache-Control': 'no-cache' });
      res.end(data);
    } catch {
      res.writeHead(404).end('Not Found');
    }
  });
}

async function waitForPass(page, timeoutMs) {
  await page.waitForFunction(
    () => {
      const statusEl = document.getElementById('status');
      if (!statusEl) return false;
      return statusEl.textContent?.startsWith('PASS:');
    },
    { timeout: timeoutMs }
  );
}

async function collectFailureContext(page) {
  const status = await page.locator('#status').textContent().catch(() => '(status unavailable)');
  const logs = await page.locator('#log').innerText().catch(() => '(log unavailable)');
  const metrics = await page.evaluate(() => window.__validationMetrics || null).catch(() => null);
  return { status, logs, metrics };
}

function buildArtifactBaseName() {
  const now = new Date().toISOString().replace(/[:.]/g, '-');
  const pageTag = PAGE_PATH.replace(/\//g, '_').replace(/[^a-zA-Z0-9_.-]/g, '');
  const queryTag = E2E_QUERY ? `_${E2E_QUERY.replace(/[^a-zA-Z0-9_.=-]/g, '_')}` : '';
  return `${now}${pageTag}${queryTag}`;
}

async function writeFailureArtifacts(page, failure) {
  await mkdir(ARTIFACT_DIR, { recursive: true });
  const base = buildArtifactBaseName();
  const screenshotPath = join(ARTIFACT_DIR, `${base}.png`);
  const detailsPath = join(ARTIFACT_DIR, `${base}.txt`);
  await page.screenshot({ path: screenshotPath, fullPage: true }).catch(() => { });
  const detailText = [
    `url=${TARGET_URL}`,
    `status=${failure.status}`,
    `metrics=${JSON.stringify(failure.metrics, null, 2)}`,
    'logs=',
    failure.logs,
  ].join('\n');
  await writeFile(detailsPath, detailText, 'utf-8');
  return { screenshotPath, detailsPath };
}

async function run() {
  const server = createStaticServer();
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(PORT, HOST, resolve);
  });

  let browser;
  try {
    browser = await chromium.launch({
      headless: HEADLESS,
      args: [
        '--use-fake-ui-for-media-stream',
        '--autoplay-policy=no-user-gesture-required',
      ],
    });

    const context = await browser.newContext();
    const page = await context.newPage();

    page.on('console', (msg) => {
      const type = msg.type();
      if (type === 'error') {
        console.error(`[browser:${type}] ${msg.text()}`);
      }
    });

    await page.goto(TARGET_URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await page.click('#btnRun', { timeout: 10000 });

    await waitForPass(page, TIMEOUT_MS);
    const status = await page.locator('#status').textContent();
    console.log(`PASS: ${status}`);

    await context.close();
  } catch (error) {
    console.error('E2E local integration test failed:', error.message);
    try {
      if (browser) {
        const context = browser.contexts()[0];
        const page = context?.pages()[0];
        if (page) {
          const failure = await collectFailureContext(page);
          console.error(`Status: ${failure.status}`);
          if (failure.metrics) {
            console.error(`Metrics: ${JSON.stringify(failure.metrics)}`);
          }
          console.error('Log:\n' + failure.logs);
          const artifacts = await writeFailureArtifacts(page, failure);
          console.error(`Artifacts: screenshot=${artifacts.screenshotPath} details=${artifacts.detailsPath}`);
        }
      }
    } catch {
      // best effort diagnostics
    }
    process.exitCode = 1;
  } finally {
    if (browser) {
      await browser.close();
    }
    await new Promise((resolve) => server.close(resolve));
  }
}

run();
