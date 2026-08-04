# Obermon

Obermon is a Chromium-derived desktop browser with a dark cyberpunk interface and a browser-native Scramjet mode.

## Repository model

This repository contains the Obermon-owned browser source and the scripts needed to materialize it into a pinned Chromium checkout. Chromium itself is too large to vendor directly, so `scripts/bootstrap.ps1` fetches the configured Chromium and Scramjet revisions, copies the Obermon overlay into the source tree, and applies the native integration edits.

Vivaldi's proprietary UI source is not included. The supplied Vivaldi installer is used only as a visual and behavioral reference. Obermon recreates the relevant layout and styling as original source code.

## Implemented source

- Chromium-native Scramjet navigation interception.
- Browser-owned Scramjet engine lifecycle.
- Destination-to-internal URL virtualization.
- A visible, non-removable Scramjet component extension.
- Requests, Playground, Settings, and the bottom mode switch.
- The built-in Obermon cyberpunk browser theme.
- The local Go engine and Wisp transport source.
- Pinned Chromium and Scramjet source bootstrap.
- Windows build and packaging scripts.

## Source layout

- `src/chromium/` — Chromium source overlay, native browser integration, component extensions, and theme.
- `native/engine/` — local Scramjet engine and Wisp server source.
- `scripts/` — source bootstrap, materialization, build, and packaging tools.
- `config/upstream.json` — pinned upstream revisions.
- `docs/` — architecture, security, design, and build documentation.

## Build

On Windows 11 with Visual Studio 2022 and at least 180 GB free:

```powershell
pwsh ./scripts/bootstrap.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/build.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/package.ps1 -WorkRoot C:\ObermonBuild
```

The repository stores source code and build tooling only. It does not contain GitHub Actions workflows, Codespaces configuration, benchmark automation, or test infrastructure.

See `docs/build-windows.md`, `docs/architecture.md`, and `docs/security-model.md`.
