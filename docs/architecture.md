# Obermon architecture

## 1. Source layout

The repository owns only Obermon-specific code. `scripts/bootstrap.ps1` materializes the pinned Chromium tree, builds the pinned Scramjet runtime, copies the Obermon source overlay into Chromium, materializes the local engine, and applies guarded native source edits.

## 2. Profile-wide browser backend

Obermon uses two profile-keyed browser services:

- `ObermonStateService` owns canonical page state, suppresses no-op changes, combines same-task updates, emits field-mask deltas, and stores bounded metadata-only request diagnostics.
- `ObermonBackendService` coordinates mediation readiness, authenticated internal navigation creation, intent hints, and the active backend implementation.

Regular and off-the-record profiles receive separate service instances. Browser windows and UI surfaces should project this shared state instead of creating independent application backends.

## 3. Browser-native Scramjet

Scramjet remains visually represented as an extension, but it is loaded with Chromium's component-extension mechanism. Component location makes it product code rather than a user-installed CRX. Obermon adds one UI exception so this component is visible in the extensions page while its disable/remove controls remain unavailable.

The component extension is the control surface. The browser process owns the mode preference, state graph, backend lifecycle, navigation interception, and visible URL mapping.

## 4. Native mode switch

`obermon.scramjet_enabled` is a registered profile preference. The built-in component extension uses Chromium's component-only `settingsPrivate` API to read and change it. `ScramjetNavigationThrottle` reads the same preference synchronously before a top-level request is dispatched.

## 5. Backend lifecycle and mediation seam

Navigation code depends on the `MediationBackend` interface rather than directly on the engine process. The active `LoopbackMediationBackend` uses `ScramjetEngineService`; a future implementation can use the Mojo contract in `chrome/browser/obermon/mojom/mediation_service.mojom`.

The transitional engine startup is an asynchronous state machine. A process handle is not considered ready. The Go engine publishes an atomic marker only after both HTTP and Wisp listeners are bound, and deferred navigations are released only after the marker appears.

## 6. URL model

Each mediated top-level navigation has two URLs:

- **destination URL**: the user-entered HTTP(S) URL and browser-visible virtual URL;
- **internal URL**: the authenticated Scramjet controller URL.

`ScramjetNavigationThrottle` defers eligible initial GET navigation, asks the profile backend to prepare an authenticated internal URL, waits for readiness, and then replaces the original navigation. `ScramjetTabHelper` validates the internal URL, assigns the destination to `NavigationEntry::SetVirtualURL`, and updates the canonical state graph.

Authorization sessions are bounded and expire. Active sessions refresh their authorization when valid internal URL changes are observed.

## 7. Intent and transport warming

The profile backend deduplicates user-intent hints and prewarms the mediation backend for strong intent. During internal document startup, destination TCP preconnect runs in parallel with service-worker and transport initialization. Warm Wisp connections are bounded, short-lived, and partitioned by the browser-generated mediation token.

## 8. Renderer and tool surfaces

Normal browsing loads only the browser surface and selected transport. Requests, Playground, Settings, Monaco, and diagnostics are separate lazy-loaded surfaces. DOM work uses delegated events, containment, cached search text, bounded lists, and animation-frame URL synchronization.

## 9. Diagnostics

The browser records bounded top-level navigation metadata without response bodies. Full subresource observation and optional body capture remain in the existing Scramjet tool until the Mojo backend provides a native streaming request-observer interface.

## 10. Security indicator

The destination URL may be displayed as the virtual URL, but Obermon must not reuse Chromium's ordinary secure-connection indicator without qualification. A dedicated mediated security state and page-info treatment remain required before release.

## 11. Vivaldi reference

Obermon recreates useful Vivaldi interaction principles—one profile-level state backend, compact browser chrome, panels, workspaces, lifecycle-aware tabs, and dense settings—as original code. No Vivaldi proprietary source is copied.

See `docs/backend-architecture.md` for implementation status and the utility-process migration plan.
