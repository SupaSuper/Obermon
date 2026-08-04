# Obermon Scramjet engine

This directory contains the browser-launched transitional loopback engine and Wisp server source.

`main.go` is kept as numbered source parts to keep large GitHub API writes reliable. `scripts/materialize.py` concatenates those parts into Chromium's generated engine directory, applies guarded integration edits, copies the runtime support files, and embeds the pinned Scramjet web build.

Runtime support:

- `runtime_ready.go` publishes an atomic readiness marker only after both loopback listeners are bound. Chromium defers mediated navigation until this marker exists.
- `runtime_pool.go` provides bounded, short-lived destination TCP preconnects. Warm connections are partitioned by the browser-generated mediation token, capped globally and per destination, and use a 350 ms speculative dial budget.

The generated executable binds only to `127.0.0.1:4141` and `127.0.0.1:4142`. Local HTTP and Wisp are compatibility boundaries for the current backend; `chrome/browser/obermon/mojom/mediation_service.mojom` describes the planned sandboxed utility-process replacement.

The engine is AGPL-3.0-or-later. The embedded Scramjet runtime remains under its upstream AGPL-3.0-only license.
