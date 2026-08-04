# Obermon

Obermon is a Chromium-derived desktop browser with a dark cyberpunk interface and a browser-native Scramjet mode.

## Repository model

Chromium is too large to vendor into this repository. This project is therefore a **source superproject**: it pins upstream Chromium and Scramjet revisions, fetches them into a local work tree, copies Obermon-owned source into Chromium, applies guarded source edits, builds Scramjet from source, and then compiles the browser.

Vivaldi's proprietary UI source is not included. The supplied Vivaldi installer is treated as a visual and behavioral reference only. Obermon recreates the relevant layout as original code.

## Intended product behavior

- Scramjet is a visible built-in component extension.
- It cannot be removed like an ordinary extension.
- Its panel contains only Requests, Playground, Settings, and a bottom on/off switch.
- When enabled, top-level navigation is mediated by Scramjet while Obermon keeps the destination URL as the browser-visible URL.
- Internal proxy URLs are not written to history, bookmarks, hover status, or the omnibox.
- Security UI must never claim that the destination's TLS identity was verified when only the local Scramjet origin was verified.

## Build status

This repository now contains the build pipeline and the first native integration implementation. A commit is not called a release until the pinned Windows build compiles and the browser tests pass on a self-hosted Windows runner.

## Start

On Windows 11 with Visual Studio 2022 and at least 180 GB free:

```powershell
pwsh ./scripts/bootstrap.ps1
pwsh ./scripts/build.ps1
pwsh ./scripts/test.ps1
pwsh ./scripts/package.ps1
```

See `docs/build-windows.md`, `docs/architecture.md`, and `docs/security-model.md`.