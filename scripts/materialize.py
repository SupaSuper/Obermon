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


def copy_scramjet(scramjet: Path, chromium: Path) -> None:
    output = chromium / "chrome/browser/resources/obermon_scramjet/vendor"
    output.mkdir(parents=True, exist_ok=True)
    candidates = [
        scramjet / "packages/core/dist",
        scramjet / "packages/controller/dist",
        scramjet / "packages/utils/dist",
    ]
    for candidate in candidates:
        if not candidate.exists():
            raise FileNotFoundError(f"Scramjet build output missing: {candidate}")
        copy_tree(candidate, output / candidate.parent.name)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--chromium", required=True, type=Path)
    parser.add_argument("--scramjet", required=True, type=Path)
    args = parser.parse_args()

    copy_tree(args.repo / "src/chromium", args.chromium)
    copy_scramjet(args.scramjet, args.chromium)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
