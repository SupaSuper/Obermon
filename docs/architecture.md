# Obermon architecture

## 1. Source layout

The repository owns only Obermon-specific code. `scripts/bootstrap.ps1` materializes the pinned Chromium tree under `.work/chromium/src`, builds Scramjet from its pinned AGPL source, copies the resulting web runtime into Chromium resources, copies `src/chromium/**` into the Chromium tree, and runs `scripts/apply_source_edits.py`.

## 2. Browser-native Scramjet

Scramjet remains visually represented as an extension, but it is loaded with Chromium's component-extension mechanism. Component location makes it product code rather than a user-installed CRX. Obermon adds one UI exception so this component is visible in the extensions page while its disable/remove controls remain unavailable.

The extension is the control surface. The browser process owns lifecycle and navigation mediation.

## 3. Engine lifecycle

`ScramjetEngineService` launches the bundled local engine with a hidden child process and a dedicated profile-scoped token. The engine binds only to loopback. Startup readiness is confirmed through a health endpoint before navigation is redirected.

The browser does not globally install an operating-system proxy and does not add a local certificate authority.

## 4. URL model

Each mediated top-level navigation has two URLs:

- **destination URL**: the user-entered HTTP(S) URL; shown in the omnibox, tabs, hover status, history, bookmarks, and session restore.
- **internal URL**: the loopback Scramjet controller URL used to load rewritten content.

`ScramjetURLMapper` creates and validates this mapping. `ScramjetTabHelper` records the destination before redirect, restores it as the `NavigationEntry` virtual URL after commit, and rejects mappings not authenticated by an in-memory navigation token.

This is not cosmetic spoofing. Obermon owns both the internal transport and the visible navigation model. The mapping is restricted to the built-in engine origin and cannot be requested by arbitrary web content.

## 5. Security indicator

The destination URL may be displayed as the virtual URL, but Obermon must not reuse Chromium's ordinary secure-connection indicator without qualification. The initial implementation uses a distinct Scramjet security state. The panel and page-info UI explain that content was fetched through Scramjet and identify the destination separately from the local document origin.

## 6. Vivaldi reference

Obermon recreates the useful Vivaldi interaction model—compact tab strip, side panel, flexible toolbar placement, and dense settings—as original HTML/CSS/Views code. No Vivaldi proprietary source is copied.
