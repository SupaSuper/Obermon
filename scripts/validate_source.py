#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import py_compile
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(command: list[str], cwd: Path | None = None) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def validate_engine(repo: Path) -> None:
    parts = sorted((repo / "native/engine/source").glob("main.go.part*"))
    if len(parts) != 4:
        raise RuntimeError(f"Expected four engine source parts, found {len(parts)}")
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        (work / "main.go").write_bytes(b"".join(part.read_bytes() for part in parts))
        shutil.copy2(repo / "native/engine/go.mod", work / "go.mod")
        web = work / "web"
        web.mkdir()
        (web / "index.html").write_text("<!doctype html><title>engine test</title>", encoding="utf-8")
        run(["gofmt", "-w", "main.go"], cwd=work)
        run(["go", "test", "./..."], cwd=work)


def validate_scripts(repo: Path) -> None:
    for script in sorted((repo / "scripts").glob("*.py")):
        py_compile.compile(str(script), doraise=True)


def validate_extension(repo: Path) -> None:
    extension = repo / "src/chromium/chrome/browser/resources/obermon_scramjet"
    manifest = json.loads((extension / "manifest.json").read_text(encoding="utf-8"))
    if manifest["name"] != "Scramjet" or manifest["manifest_version"] != 3:
        raise RuntimeError("Unexpected built-in extension manifest")
    for script in ("background.js", "popup.js"):
        run(["node", "--check", str(extension / script)])
    popup = (extension / "popup.html").read_text(encoding="utf-8")
    for label in ("Requests", "Playground", "Settings"):
        if label not in popup:
            raise RuntimeError(f"Popup is missing {label}")
    if "enabled" not in (extension / "background.js").read_text(encoding="utf-8"):
        raise RuntimeError("The Scramjet toggle implementation is missing")


def validate_layout(repo: Path) -> None:
    required = [
        "config/upstream.json",
        "scripts/bootstrap.ps1",
        "scripts/build.ps1",
        "scripts/test.ps1",
        "scripts/package.ps1",
        "src/chromium/chrome/browser/obermon/scramjet_tab_helper.cc",
        "src/chromium/chrome/browser/obermon/scramjet_url_mapper.cc",
        "src/chromium/chrome/browser/obermon/scramjet_engine_service.cc",
    ]
    missing = [path for path in required if not (repo / path).exists()]
    if missing:
        raise RuntimeError("Missing required files: " + ", ".join(missing))
    json.loads((repo / "config/upstream.json").read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    repo = args.repo.resolve()
    validate_layout(repo)
    validate_scripts(repo)
    validate_extension(repo)
    validate_engine(repo)
    print("Obermon source validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
