# Obermon backend architecture (`experiment`)

This document describes the source architecture currently implemented on the `experiment` branch and the remaining migration from the transitional loopback backend to a Chromium utility-process backend.

## Implemented now

### Profile-scoped state graph

`ObermonStateService` is a `ProfileKeyedService` with separate regular and off-the-record instances. It owns canonical state for every attached `WebContents`:

- destination and internal URLs;
- lifecycle, mediation, and loading state;
- revision number;
- audible, pinned, and unsaved-form flags;
- a bounded metadata-only request ring.

Mutations are sparse. Equal assignments are ignored, effective changes are represented by a field mask, and multiple changes during the same browser task are combined before observers are notified. Windows and UI surfaces should subscribe to the fields they render rather than maintaining independent copies of tab state.

### Profile backend coordinator

`ObermonBackendService` is the browser-side entry point for:

- backend readiness;
- authenticated internal navigation creation;
- destination intent hints;
- mediation failure reporting;
- access to the profile state graph.

Each service generates one random, in-memory transport partition for its profile lifetime. Regular and off-the-record profiles therefore use different transport identities. Per-navigation authorization remains a separate bounded token and is never reused as the profile pool identity.

The coordinator owns a `MediationBackend` interface. The active implementation is `LoopbackMediationBackend`, but navigation and tab code no longer depend directly on the Go process. This is the replacement seam for the future Mojo backend.

### Readiness-gated navigation

`ScramjetNavigationThrottle` no longer treats a valid process handle as a ready backend. It defers the original navigation, requests readiness from the profile coordinator, and redirects only after the engine has atomically published that both local listeners are bound. Startup failure cancels the mediated navigation and records a failed mediation state. A crashed engine closes its stale process handle and can be started again.

### One initialized transport per profile configuration

Normal tabs no longer instantiate independent Libcurl or Epoxy clients. Each document creates a lightweight `SharedTransportClient`, while `shared-transport-worker.ts` owns the initialized transport instances.

The worker is scoped by Chromium's profile storage partition and caches transports by:

```text
profile transport partition
+ selected transport implementation
+ partitioned Wisp URL
```

HTTP requests and WebSocket streams are multiplexed through per-tab `MessagePort` connections. Abort signals propagate into worker-owned `AbortController` instances. Page shutdown closes its outstanding streams, while other tabs continue using the same underlying transport.

When `SharedWorker` is unavailable or cannot initialize, Obermon falls back to a direct transport for that document.

### Intent and connection warming

Intent hints are deduplicated in the profile backend. Selection and committed navigation prewarm the shared engine without creating a renderer.

When the internal Scramjet document begins startup, it issues a speculative `/preconnect` request in parallel with service-worker registration and shared-transport initialization. The request and engine dial each have a 350 ms budget and do not sit on the controller's critical path. The Go engine temporarily retains a raw destination TCP connection for the profile transport partition. A Wisp stream from the same partition can consume that connection.

The warm pool is:

- bounded to two connections per destination and partition;
- bounded to 64 connections globally;
- expired after twelve seconds;
- unavailable across different profile transport partitions;
- optional and non-blocking on failure.

### Bounded diagnostics

The browser records top-level navigation metadata outside page JavaScript. Records are bounded and contain no response bodies. Body capture and full subresource diagnostics remain in the existing Scramjet tool until a Mojo request-observer stream is wired.

### Authorization lifecycle

Virtual URL authorization tokens are separate from the profile transport partition. Navigation tokens are bounded and time-limited, while active sessions refresh authorization when a valid internal URL update is observed.

## Active runtime shape

```text
Chromium browser process
  ObermonStateService (one per profile / OTR profile)
  ObermonBackendService (one per profile / OTR profile)
  stable in-memory profile transport partition
  ScramjetNavigationThrottle
  ScramjetTabHelper
                 |
                 | MediationBackend interface
                 v
LoopbackMediationBackend
  ScramjetEngineService readiness state machine
                 |
                 | local HTTP + partitioned Wisp
                 v
Go engine
  bounded warm TCP pool
  Scramjet web runtime
                 ^
                 | one profile-scoped SharedWorker transport
                 | lightweight MessagePort clients per tab
```

The local HTTP and Wisp boundary is still transitional. The browser-visible destination remains a virtual URL and must use a distinct mediated security indicator.

## Utility-process migration contract

`src/chromium/chrome/browser/obermon/mojom/mediation_service.mojom` defines the intended next backend:

```text
MediationService
  CreateSession
  Preconnect
  GetHealth

MediationSession
  Navigate
  Freeze
  Resume
  Close
```

Responses are designed to stream through Mojo data pipes. The contract is source-visible but deliberately not added to the active build yet; adding an interface without a sandboxed service implementation would create a false sense that localhost IPC had been removed.

## Next implementation slices

1. Implement `MediationService` in a sandboxed Chromium utility process.
2. Add a Mojo-backed `MediationBackend` implementation and select it behind a feature flag.
3. Move response rewriting and request metadata streaming into that service.
4. Replace the localhost controller URL with an internal browser-owned URL loader.
5. Connect page lifecycle changes to `MediationSession.Freeze/Resume/Close` and Chromium Performance Manager.
6. Replace the normal browsing shell with a minimal renderer agent; keep Requests, Playground, Settings, and Monaco in separately loaded tool surfaces.
7. Define one coordinated raw-response and rewritten-output cache policy.
8. Move omnibox ranking and index providers behind a revisioned, cancellation-aware native service.

## Invariants

- `main` remains unchanged until experiment code is compiled and reviewed.
- Private profiles do not share the profile state graph, intent history, or shared transport worker.
- A virtual destination is accepted only with a browser-generated navigation token.
- The stable profile transport partition cannot authorize a virtual destination.
- Speculative warming never creates a destination document or writes history.
- Diagnostics do not copy response bodies by default.
- The normal HTTPS indicator must not represent a mediated localhost document as a directly verified destination connection.
