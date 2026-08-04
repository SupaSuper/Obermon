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


def patch_engine_readiness(main_go: Path) -> None:
    source = main_go.read_text(encoding="utf-8")
    ready_call = (
        '\tif err := writeReadyFile(httpListener.Addr().String(), '
        'wispListener.Addr().String()); err != nil {\n'
        '\t\t_ = httpListener.Close()\n'
        '\t\t_ = wispListener.Close()\n'
        '\t\treturn err\n'
        '\t}\n\n'
    )
    if ready_call in source:
        return

    anchor = (
        '\tlog.Printf("Scramjet frontend listening on http://%s/", '
        'httpAddress)\n'
    )
    if anchor not in source:
        raise RuntimeError(
            "Pinned native engine source changed; readiness anchor is missing"
        )
    main_go.write_text(source.replace(anchor, ready_call + anchor, 1), encoding="utf-8")


def materialize_engine(repo: Path, chromium: Path, scramjet: Path) -> None:
    generated = chromium / "chrome/browser/obermon/generated_engine"
    if generated.exists():
        shutil.rmtree(generated)
    generated.mkdir(parents=True)

    parts = sorted((repo / "native/engine/source").glob("main.go.part*"))
    if not parts:
        raise FileNotFoundError("No native engine source parts were found")
    main_go = generated / "main.go"
    with main_go.open("wb") as output:
        for part in parts:
            output.write(part.read_bytes())

    patch_engine_readiness(main_go)
    shutil.copy2(repo / "native/engine/runtime_ready.go", generated / "runtime_ready.go")
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
