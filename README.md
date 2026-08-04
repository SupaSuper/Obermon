# Obermon

Obermon is a Chromium-derived desktop browser with a dark cyberpunk interface and a browser-native Scramjet mode.

## Repository model

Chromium is too large to vendor into this repository. This project is therefore a **source superproject**: it pins upstream Chromium and Scramjet revisions, fetches them into a local work tree, copies Obermon-owned source into Chromium, applies guarded source edits, builds Scramjet from source, and then compiles the browser.

Vivaldi's proprietary UI source is not included. The supplied Vivaldi installer is treated as a visual and behavioral reference only. Obermon recreates the relevant layout as original code.

## Intended product behavior

- Scramjet is a visible built-in component extension.
- It cannot be removed like an ordinary extension.
- Its panel contains only Requests, Playground, Settings, and a bottom on/off switch.
- When enabled, top-level navigation is mediated by the full pinned Scramjet runtime while Obermon keeps the destination URL as the browser-visible virtual URL.
- Internal proxy URLs are not intended to be exposed through the omnibox or normal navigation UI.
- Security UI must never claim that the destination's TLS identity was verified when only the local Scramjet origin was verified.

## What is implemented in source

- Pinned Chromium and Scramjet bootstrap.
- Full Scramjet demo/runtime build and local Wisp engine embedding.
- Browser-owned engine lifecycle.
- A visible component extension with the finalized Requests / Playground / Settings panel and mode switch.
- Top-level mediation and `NavigationEntry::SetVirtualURL` destination mapping.
- Guarded Chromium source materialization, Windows build/test/package scripts, and source validation CI.

## Build status

The source implementation is present, but no binary is described as a release until the pinned Windows Chromium build compiles and the end-to-end browser tests pass on a self-hosted Windows runner. This distinction is intentional.

## Start

On Windows 11 with Visual Studio 2022 and at least 180 GB free:

```powershell
pwsh ./scripts/bootstrap.ps1
pwsh ./scripts/build.ps1
pwsh ./scripts/test.ps1
pwsh ./scripts/package.ps1
```

See `docs/build-windows.md`, `docs/architecture.md`, and `docs/security-model.md`.
