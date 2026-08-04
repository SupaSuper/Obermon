# Obermon architecture

## 1. Source layout

The repository owns only Obermon-specific code. `scripts/bootstrap.ps1` materializes the pinned Chromium tree under `.work/chromium/src`, builds Scramjet from its pinned AGPL source, copies the resulting web runtime into the local engine, copies `src/chromium/**` into Chromium, and runs guarded source edits.

## 2. Browser-native Scramjet

Scramjet remains visually represented as an extension, but it is loaded with Chromium's component-extension mechanism. Component location makes it product code rather than a user-installed CRX. Obermon adds one UI exception so this component is visible in the extensions page while its disable/remove controls remain unavailable.

The component extension is the control surface. The browser process owns the mode preference, engine lifecycle, navigation interception, and visible URL mapping.

## 3. Native mode switch

`obermon.scramjet_enabled` is a registered profile preference. The built-in component extension uses Chromium's component-only `settingsPrivate` API to read and change it. `ScramjetNavigationThrottle` reads the same preference synchronously before a top-level request is dispatched. This removes the earlier race where an extension redirected a request only after browser navigation began.

## 4. Engine lifecycle

`ScramjetEngineService` launches the bundled local engine with a hidden child process. The engine binds only to loopback. The browser does not globally install an operating-system proxy and does not add a local certificate authority.

## 5. URL model

Each mediated top-level navigation has two URLs:

- **destination URL**: the user-entered HTTP(S) URL; used as the browser-visible virtual URL.
- **internal URL**: the loopback Scramjet controller URL used to load rewritten content.

`ScramjetNavigationThrottle` cancels eligible initial GET navigation before dispatch and opens the internal controller URL. `ScramjetTabHelper` validates the internal origin and extracts the destination, assigns it to `NavigationEntry::SetVirtualURL`, and invalidates the browser URL state so the omnibox updates.

Subsequent fetches, workers, WebSockets, links, and form submissions inside the mediated page are handled by the pinned Scramjet runtime. Initial browser-level POST navigation is deliberately allowed to proceed directly rather than silently discarding a body.

## 6. Security indicator

The destination URL may be displayed as the virtual URL, but Obermon must not reuse Chromium's ordinary secure-connection indicator without qualification. A dedicated mediated security state and page-info treatment remain required before release.

## 7. Vivaldi reference

Obermon recreates the useful Vivaldi interaction model—compact tab strip, side panel, flexible toolbar placement, and dense settings—as original code. No Vivaldi proprietary source is copied.
