# Obermon

Obermon is a Chromium-derived browser project with a dark cyberpunk interface and a browser-native Scramjet mode.

This repository contains Obermon-owned source, Chromium patch overlays, the built-in Scramjet component extension, native integration code, tests, and reproducible Windows build tooling. It intentionally does not vendor Chromium's multi-gigabyte upstream source tree or Vivaldi's proprietary browser UI. The bootstrap script pins and fetches upstream Chromium, applies Obermon's source overlay, and builds the resulting browser.

> Status: active source implementation. A successful Windows compile and end-to-end browser test is required before any artifact is called a release.

See `docs/architecture.md` and `docs/build-windows.md` after the bootstrap commit lands.
