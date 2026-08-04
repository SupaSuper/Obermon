# GitHub Codespaces

Obermon includes a Codespaces devcontainer for cloud-based source development and validation.

## What runs in Codespaces

When the codespace is created, `scripts/codespace-check.sh` automatically validates:

- Obermon repository structure and Python tooling;
- the reconstructed Go Scramjet engine source and its unit compilation;
- component-extension JavaScript and JSON manifests;
- the Speedometer automation script syntax;
- shell script quality checks.

Run it again at any time:

```bash
bash scripts/codespace-check.sh
```

## What does not run there

GitHub Codespaces uses a Linux development container. Obermon's release target and benchmark workflow currently require Windows, Visual Studio, a large Chromium work tree, and an unlocked interactive desktop. The Codespace therefore does not:

- compile the Windows `obermon.exe` release;
- replace the `obermon-builder` self-hosted Windows runner;
- produce a release-grade BrowserBench Speedometer result.

The full build and Speedometer comparison remain in `.github/workflows/windows-self-hosted.yml`.

## Recommended machine

The devcontainer requests at least 8 CPU cores, 16 GB RAM, and 64 GB storage. That is intended for editing and source validation, not a full Chromium checkout.
