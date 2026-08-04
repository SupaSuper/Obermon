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


def materialize_engine(repo: Path, chromium: Path, scramjet: Path) -> None:
    generated = chromium / "chrome/browser/obermon/generated_engine"
    if generated.exists():
        shutil.rmtree(generated)
    generated.mkdir(parents=True)

    parts = sorted((repo / "native/engine/source").glob("main.go.part*"))
    if not parts:
        raise FileNotFoundError("No native engine source parts were found")
    with (generated / "main.go").open("wb") as output:
        for part in parts:
            output.write(part.read_bytes())

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
