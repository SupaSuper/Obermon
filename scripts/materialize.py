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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--chromium", required=True, type=Path)
    parser.add_argument("--scramjet", required=True, type=Path)
    args = parser.parse_args()

    copy_tree(args.repo / "src/chromium", args.chromium)

    demo_dist = args.scramjet / "packages/demo/dist"
    engine_web = args.repo / "native/engine/web"
    if engine_web.exists():
        shutil.rmtree(engine_web)
    copy_tree(demo_dist, engine_web)

    # The engine source is built outside GN, then copied beside the browser.
    generated_engine = args.chromium / "chrome/browser/obermon/generated_engine"
    if generated_engine.exists():
        shutil.rmtree(generated_engine)
    copy_tree(args.repo / "native/engine", generated_engine)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
