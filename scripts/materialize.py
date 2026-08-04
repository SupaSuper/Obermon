#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def copy_tree(source: Path, destination: Path) -> None:
    if not source.exists():
        raise FileNotFoundError(source)
    for item in source.rglob("*"):
        relative = item.relative_to(source)
        target = destination / relative
        if item.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)


def replace_once(source: str, old: str, new: str, description: str) -> str:
    if new in source:
        return source
    if old not in source:
        raise RuntimeError(
            f"Pinned native engine source changed; {description} anchor is missing"
        )
    return source.replace(old, new, 1)


def patch_engine_readiness(source: str) -> str:
    existing_engine_anchor = (
        '\t\tif engineHealthy() {\n'
        '\t\t\t// Another installed host already owns the ports. Keep the native host\n'
        '\t\t\t// alive so the extension retains a valid port connection.\n'
        '\t\t\tselect {}\n'
        '\t\t}\n'
    )
    existing_engine_replacement = (
        '\t\tif engineHealthy() {\n'
        '\t\t\t// Another installed host already owns the ports. Publish readiness\n'
        '\t\t\t// for the browser that launched this compatibility process, then\n'
        '\t\t\t// remain alive so its process handle stays valid.\n'
        '\t\t\tif readyErr := writeReadyFile(httpAddress, wispAddress); readyErr != nil {\n'
        '\t\t\t\treturn readyErr\n'
        '\t\t\t}\n'
        '\t\t\tselect {}\n'
        '\t\t}\n'
    )
    source = replace_once(
        source,
        existing_engine_anchor,
        existing_engine_replacement,
        "existing-engine readiness",
    )

    ready_call = (
        '\tif err := writeReadyFile(httpListener.Addr().String(), '
        'wispListener.Addr().String()); err != nil {\n'
        '\t\t_ = httpListener.Close()\n'
        '\t\t_ = wispListener.Close()\n'
        '\t\treturn err\n'
        '\t}\n\n'
    )
    anchor = (
        '\tlog.Printf("Scramjet frontend listening on http://%s/", '
        'httpAddress)\n'
    )
    return replace_once(source, anchor, ready_call + anchor, "listener readiness")


def patch_engine_pool(source: str) -> str:
    source = replace_once(
        source,
        '\t\tif r.URL.Path == "/health" {\n',
        '\t\tif handlePreconnect(w, r) {\n'
        '\t\t\treturn\n'
        '\t\t}\n\n'
        '\t\tif r.URL.Path == "/health" {\n',
        "preconnect handler",
    )
    source = replace_once(
        source,
        '\tsession := newWispSession(ws, protocol != "")\n',
        '\tpartition := normalizedPartition(r.URL.Query().Get("partition"))\n'
        '\tsession := newWispSession(ws, protocol != "", partition)\n',
        "Wisp partition selection",
    )
    source = replace_once(
        source,
        'type wispSession struct {\n\tws      *websocketConn\n\tversion int\n',
        'type wispSession struct {\n\tws        *websocketConn\n\tversion   int\n\tpartition string\n',
        "Wisp session partition field",
    )
    source = replace_once(
        source,
        'func newWispSession(ws *websocketConn, v2 bool) *wispSession {\n',
        'func newWispSession(ws *websocketConn, v2 bool, partition string) *wispSession {\n',
        "Wisp session constructor",
    )
    source = replace_once(
        source,
        '\treturn &wispSession{\n\t\tws:      ws,\n\t\tversion: version,\n',
        '\treturn &wispSession{\n\t\tws:        ws,\n\t\tversion:   version,\n\t\tpartition: partition,\n',
        "Wisp session partition initialization",
    )
    source = replace_once(
        source,
        '\tconn, err := net.DialTimeout(network, address, 20*time.Second)\n',
        '\tconn, err := warmConnections.takeOrDial(\n'
        '\t\tst.session.partition, network, address, 20*time.Second)\n',
        "warm connection consumption",
    )
    return source


def materialize_engine(repo: Path, chromium: Path, scramjet: Path) -> None:
    generated = chromium / "chrome/browser/obermon/generated_engine"
    if generated.exists():
        shutil.rmtree(generated)
    generated.mkdir(parents=True)

    parts = sorted((repo / "native/engine/source").glob("main.go.part*"))
    if not parts:
        raise FileNotFoundError("No native engine source parts were found")
    main_go = generated / "main.go"
    source = b"".join(part.read_bytes() for part in parts).decode("utf-8")
    source = patch_engine_readiness(source)
    source = patch_engine_pool(source)
    main_go.write_text(source, encoding="utf-8")

    for runtime_file in ("runtime_ready.go", "runtime_pool.go"):
        shutil.copy2(repo / "native/engine" / runtime_file, generated / runtime_file)
    shutil.copy2(repo / "native/engine/go.mod", generated / "go.mod")
    shutil.copy2(repo / "native/engine/LICENSE", generated / "LICENSE")

    demo_dist = scramjet / "packages/demo/dist"
    if not (demo_dist / "index.html").exists():
        raise FileNotFoundError(
            f"Pinned Scramjet demo build output missing: {demo_dist}"
        )
    copy_tree(demo_dist, generated / "web")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--chromium", required=True, type=Path)
    parser.add_argument("--scramjet", required=True, type=Path)
    args = parser.parse_args()

    copy_tree(args.repo / "src/chromium", args.chromium)
    materialize_engine(args.repo, args.chromium, args.scramjet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
