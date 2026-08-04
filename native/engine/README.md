# Obermon Scramjet engine

This directory contains the browser-launched loopback engine and Wisp server source.

`main.go` is kept as numbered source parts to keep large GitHub API writes reliable. `scripts/materialize.py` concatenates the parts byte-for-byte into Chromium's generated engine directory, copies `go.mod`, and adds the pinned Scramjet demo build as the embedded `web/` tree. The resulting executable binds only to `127.0.0.1:4141` and `127.0.0.1:4142`.

The engine is AGPL-3.0-or-later. The embedded Scramjet runtime remains under its upstream AGPL-3.0-only license.
