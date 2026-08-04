import { chromium } from "playwright-core";
import { spawn, spawnSync } from "node:child_process";
import { once } from "node:events";
import { createWriteStream } from "node:fs";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";

const SCRAMJET_EXTENSION_ID = "nfmkpakigincnlglfeddmombloaeikci";
const SCRAMJET_POPUP_URL = `chrome-extension://${SCRAMJET_EXTENSION_ID}/popup.html`;
const SPEEDOMETER_URL =
  "https://browserbench.org/Speedometer3.1/?startAutomatically&iterationCount=10&viewport=1200x750";
const DEFAULT_TIMEOUT_MS = 30 * 60 * 1000;

function parseArguments(argv) {
  const options = {
    browser: "",
    output: path.resolve("benchmark-results"),
    runs: 3,
    cooldownMs: 45_000,
  };
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    const value = argv[index + 1];
    if (argument === "--browser") {
      options.browser = path.resolve(value);
      index += 1;
    } else if (argument === "--output") {
      options.output = path.resolve(value);
      index += 1;
    } else if (argument === "--runs") {
      options.runs = Number.parseInt(value, 10);
      index += 1;
    } else if (argument === "--cooldown-ms") {
      options.cooldownMs = Number.parseInt(value, 10);
      index += 1;
    } else {
      throw new Error(`Unknown or incomplete argument: ${argument}`);
    }
  }
  if (!options.browser) throw new Error("--browser is required");
  if (!Number.isInteger(options.runs) || options.runs < 1 || options.runs > 10) {
    throw new Error("--runs must be an integer from 1 through 10");
  }
  if (!Number.isInteger(options.cooldownMs) || options.cooldownMs < 0) {
    throw new Error("--cooldown-ms must be a non-negative integer");
  }
  return options;
}

function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function reservePort() {
  const server = net.createServer();
  server.unref();
  server.listen(0, "127.0.0.1");
  await once(server, "listening");
  const address = server.address();
  const port = typeof address === "object" && address ? address.port : 0;
  await new Promise((resolve, reject) =>
    server.close((error) => (error ? reject(error) : resolve())),
  );
  if (!port) throw new Error("Could not reserve a DevTools port");
  return port;
}

async function waitForDevTools(port, child, timeoutMs = 60_000) {
  const endpoint = `http://127.0.0.1:${port}`;
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (child.exitCode !== null) {
      throw new Error(`Browser exited before DevTools became ready: ${child.exitCode}`);
    }
    try {
      const response = await fetch(`${endpoint}/json/version`, {
        signal: AbortSignal.timeout(1_000),
      });
      if (response.ok) return { endpoint, version: await response.json() };
    } catch {
      // Browser startup races are expected here.
    }
    await sleep(250);
  }
  throw new Error(`DevTools did not become ready at ${endpoint}`);
}

async function waitForScramjetControl(page) {
  const deadline = Date.now() + 60_000;
  let lastError;
  while (Date.now() < deadline) {
    try {
      await page.goto(SCRAMJET_POPUP_URL, {
        waitUntil: "domcontentloaded",
        timeout: 15_000,
      });
      const toggle = page.locator("#toggle");
      await toggle.waitFor({ state: "visible", timeout: 10_000 });
      await page.waitForFunction(
        () =>
          document.querySelector("#toggle")?.checked === true ||
          document.querySelector("#toggle")?.checked === false,
        undefined,
        { timeout: 10_000 },
      );
      return toggle;
    } catch (error) {
      lastError = error;
      await sleep(500);
    }
  }
  throw new Error(`Scramjet component extension did not become ready: ${lastError}`);
}

async function setScramjetMode(page, enabled) {
  const toggle = await waitForScramjetControl(page);
  const current = await toggle.isChecked();
  if (current !== enabled) {
    await toggle.click();
    await page.waitForFunction(
      (expected) => document.querySelector("#toggle")?.checked === expected,
      enabled,
      { timeout: 20_000 },
    );
    await page.waitForTimeout(750);
  }
}

async function findSpeedometerFrame(page, timeoutMs = 120_000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    for (const frame of page.frames()) {
      try {
        const found = await frame.evaluate(
          () => Boolean(document.querySelector("#result-number")),
        );
        if (found) return frame;
      } catch {
        // A frame may detach while Scramjet creates its controlled frame.
      }
    }
    await sleep(250);
  }
  throw new Error("Could not find the Speedometer benchmark frame");
}

function processTreeKill(pid) {
  if (!pid) return;
  if (process.platform === "win32") {
    spawnSync("taskkill", ["/PID", String(pid), "/T", "/F"], {
      stdio: "ignore",
      windowsHide: true,
    });
  } else {
    try {
      process.kill(-pid, "SIGKILL");
    } catch {
      try {
        process.kill(pid, "SIGKILL");
      } catch {
        // Process already exited.
      }
    }
  }
}

async function runOne(options, mode, runNumber) {
  const runName = `${mode}-run-${String(runNumber).padStart(2, "0")}`;
  const runDirectory = path.join(options.output, runName);
  await mkdir(runDirectory, { recursive: true });
  const profile = await mkdtemp(path.join(os.tmpdir(), `obermon-${runName}-`));
  const port = await reservePort();
  const browserLog = createWriteStream(path.join(runDirectory, "browser.log"), {
    flags: "w",
  });
  const child = spawn(
    options.browser,
    [
      `--remote-debugging-port=${port}`,
      `--user-data-dir=${profile}`,
      "--no-first-run",
      "--no-default-browser-check",
      "--disable-background-networking",
      "--disable-component-update",
      "--disable-sync",
      "--metrics-recording-only",
      "--window-size=1280,900",
      "--new-window",
      "about:blank",
    ],
    {
      detached: process.platform !== "win32",
      stdio: ["ignore", browserLog, browserLog],
      windowsHide: false,
    },
  );

  let browser;
  try {
    const devTools = await waitForDevTools(port, child);
    browser = await chromium.connectOverCDP(devTools.endpoint);
    const context = browser.contexts()[0];
    if (!context) throw new Error("Obermon did not expose a default browser context");
    const page = context.pages()[0] ?? (await context.newPage());
    page.setDefaultTimeout(60_000);
    await page.bringToFront();

    await setScramjetMode(page, mode === "scramjet");
    await page.bringToFront();

    try {
      await page.goto(SPEEDOMETER_URL, {
        waitUntil: "domcontentloaded",
        timeout: 120_000,
      });
    } catch (error) {
      if (mode !== "scramjet" || !String(error).includes("ERR_ABORTED")) throw error;
    }

    const benchmarkFrame = await findSpeedometerFrame(page);
    await page.bringToFront();
    await benchmarkFrame.waitForFunction(
      () => ["DONE", "ERROR"].includes(document.body?.dataset?.benchmarkState),
      undefined,
      { timeout: DEFAULT_TIMEOUT_MS },
    );

    const result = await benchmarkFrame.evaluate(() => {
      const client = globalThis.benchmarkClient;
      const state = document.body?.dataset?.benchmarkState ?? "UNKNOWN";
      const scoreText =
        document.querySelector("#result-number")?.textContent?.trim() ?? "";
      const confidenceText =
        document.querySelector("#confidence-number")?.textContent?.trim() ?? "";
      let modern = null;
      let classic = null;
      if (client && typeof client._formattedJSONResult === "function") {
        modern = JSON.parse(client._formattedJSONResult({ modern: true }));
        classic = JSON.parse(client._formattedJSONResult({ modern: false }));
      }
      return {
        state,
        scoreText,
        confidenceText,
        modern,
        classic,
        benchmarkUrl: location.href,
        userAgent: navigator.userAgent,
      };
    });

    const score = Number.parseFloat(result.scoreText);
    if (result.state !== "DONE" || !Number.isFinite(score) || score <= 0) {
      throw new Error(
        `Speedometer failed: state=${result.state}, score=${result.scoreText}`,
      );
    }

    const record = {
      mode,
      runNumber,
      score,
      confidence: result.confidenceText,
      speedometer: result,
      browser: devTools.version,
      timestamp: new Date().toISOString(),
    };
    await writeFile(
      path.join(runDirectory, "result.json"),
      `${JSON.stringify(record, null, 2)}\n`,
      "utf8",
    );
    await page.screenshot({
      path: path.join(runDirectory, "result.png"),
      fullPage: true,
    });
    return record;
  } finally {
    try {
      await browser?.close();
    } catch {
      // Continue with a process-tree kill below.
    }
    processTreeKill(child.pid);
    browserLog.end();
    await rm(profile, { recursive: true, force: true });
  }
}

function statistics(records) {
  const scores = records.map((record) => record.score).sort((a, b) => a - b);
  const total = scores.reduce((sum, score) => sum + score, 0);
  const middle = Math.floor(scores.length / 2);
  const median =
    scores.length % 2 === 0
      ? (scores[middle - 1] + scores[middle]) / 2
      : scores[middle];
  return {
    runs: scores.length,
    scores,
    mean: total / scores.length,
    median,
    minimum: scores[0],
    maximum: scores.at(-1),
  };
}

function markdown(summary) {
  const direct = summary.results.direct;
  const scramjet = summary.results.scramjet;
  const slowdown = summary.comparison.scramjetSlowdownPercent;
  return (
    `# Obermon Speedometer 3.1\n\n` +
    `Higher scores are better. Each run uses Speedometer's standard 10 iterations.\n\n` +
    `| Mode | Runs | Mean | Median | Min | Max |\n` +
    `|---|---:|---:|---:|---:|---:|\n` +
    `| Direct | ${direct.runs} | ${direct.mean.toFixed(3)} | ${direct.median.toFixed(3)} | ${direct.minimum.toFixed(3)} | ${direct.maximum.toFixed(3)} |\n` +
    `| Scramjet | ${scramjet.runs} | ${scramjet.mean.toFixed(3)} | ${scramjet.median.toFixed(3)} | ${scramjet.minimum.toFixed(3)} | ${scramjet.maximum.toFixed(3)} |\n\n` +
    `Scramjet score ratio: **${(summary.comparison.scramjetToDirectRatio * 100).toFixed(2)}%** of direct mode.\n\n` +
    `Measured Scramjet slowdown: **${slowdown.toFixed(2)}%**. A negative value means Scramjet scored higher in this sample.\n`
  );
}

async function main() {
  const options = parseArguments(process.argv.slice(2));
  await mkdir(options.output, { recursive: true });
  const records = { direct: [], scramjet: [] };

  for (const mode of ["direct", "scramjet"]) {
    for (let runNumber = 1; runNumber <= options.runs; runNumber += 1) {
      console.log(`Running Speedometer 3.1: ${mode}, run ${runNumber}/${options.runs}`);
      const record = await runOne(options, mode, runNumber);
      records[mode].push(record);
      console.log(`${mode} run ${runNumber}: ${record.score}`);
      const isLast = mode === "scramjet" && runNumber === options.runs;
      if (!isLast && options.cooldownMs > 0) await sleep(options.cooldownMs);
    }
  }

  const direct = statistics(records.direct);
  const scramjet = statistics(records.scramjet);
  const ratio = scramjet.mean / direct.mean;
  const summary = {
    benchmark: {
      name: "Speedometer 3.1",
      url: SPEEDOMETER_URL,
      iterationsPerRun: 10,
      runsPerMode: options.runs,
    },
    machine: {
      platform: os.platform(),
      release: os.release(),
      architecture: os.arch(),
      cpu: os.cpus()[0]?.model ?? "unknown",
      logicalCpuCount: os.cpus().length,
      totalMemoryBytes: os.totalmem(),
      runnerName: process.env.RUNNER_NAME ?? null,
      runnerEnvironment: process.env.RUNNER_ENVIRONMENT ?? null,
    },
    browserPath: options.browser,
    results: { direct, scramjet },
    comparison: {
      scramjetToDirectRatio: ratio,
      scramjetSlowdownPercent: (1 - ratio) * 100,
    },
    records,
    generatedAt: new Date().toISOString(),
  };

  await writeFile(
    path.join(options.output, "summary.json"),
    `${JSON.stringify(summary, null, 2)}\n`,
    "utf8",
  );
  await writeFile(
    path.join(options.output, "REPORT.md"),
    markdown(summary),
    "utf8",
  );
  console.log(markdown(summary));
}

main().catch((error) => {
  console.error(error?.stack ?? error);
  process.exitCode = 1;
});
