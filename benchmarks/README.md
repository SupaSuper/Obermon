# Obermon performance benchmarks

The Windows build workflow runs the official hosted **BrowserBench Speedometer 3.1** benchmark against the built Obermon executable in two isolated clean profiles:

1. **Direct mode** — the built-in Scramjet preference is disabled through the component extension's native preference API.
2. **Scramjet mode** — the same browser build runs with browser-native Scramjet mediation enabled.

Each sample uses Speedometer's standard ten internal iterations. The workflow performs three independent samples per mode by default, restarting Obermon with a new profile and allowing a cooldown between samples.

## Accuracy requirements

The self-hosted Windows runner must:

- run in an unlocked interactive desktop session;
- have no other browser windows or heavy background programs open;
- use consistent power and thermal conditions;
- keep the benchmark window focused;
- use the same physical machine for both modes.

Virtualized or shared GitHub-hosted runners are unsuitable for release performance claims because their CPU scheduling and available graphics acceleration can vary.

## Local invocation

After building Obermon:

```powershell
npm install --prefix benchmarks --ignore-scripts --no-audit --no-fund
node benchmarks/run-speedometer.mjs `
  --browser C:\ObermonBuild\chromium\src\out\ObermonRelease\chrome.exe `
  --output C:\ObermonBuild\benchmark-results `
  --runs 3
```

The output contains a JSON record and screenshot for every run, plus `summary.json` and `REPORT.md`.
