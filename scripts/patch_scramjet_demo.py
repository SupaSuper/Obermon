#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"Pinned Scramjet source changed; anchor missing in {path}: {old[:80]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scramjet", required=True, type=Path)
    args = parser.parse_args()
    app = args.scramjet / "packages/demo/src/App.tsx"
    browser = args.scramjet / "packages/demo/src/pages/BrowserView.tsx"

    replace_once(
        app,
        'this.activeTab ??= "browser";\n\treturn (\n\t\t<div>',
        'const requestedTool = new URL(location.href).searchParams.get("tool");\n\tthis.activeTab ??= requestedTool === "requests" || requestedTool === "playground" || requestedTool === "settings"\n\t\t? requestedTool\n\t\t: "browser";\n\treturn (\n\t\t<div class="obermon-hosted">',
    )
    replace_once(
        app,
        '\t.top-bar {\n',
        '\t/* Obermon owns the real browser chrome. The Scramjet demo is used only\n\t   as a full-viewport renderer or a single requested tool page. */\n\t.obermon-hosted .top-bar { display: none; }\n\n\t.top-bar {\n',
    )
    replace_once(
        browser,
        'let urlWatcher = new UrlWatcherPlugin((url) => {\n\t\t\tbrowserState.url = url;\n\t\t});',
        'let urlWatcher = new UrlWatcherPlugin((url) => {\n\t\t\tbrowserState.url = url;\n\t\t\tconst shellUrl = new URL(location.href);\n\t\t\tshellUrl.searchParams.set("goto", url);\n\t\t\tshellUrl.searchParams.set("obermon", "1");\n\t\t\thistory.replaceState(null, "", shellUrl);\n\t\t});',
    )
    replace_once(
        browser,
        'let catchEscapedLinks = new CatchEscapedLinksPlugin(\n\t\t\t(url) =>\n\t\t\t\tnew URL(`/?goto=${encodeURIComponent(url.href)}`, location.origin)\n\t\t);',
        'let catchEscapedLinks = new CatchEscapedLinksPlugin((url) => {\n\t\t\tconst shellUrl = new URL(location.href);\n\t\t\tshellUrl.searchParams.set("goto", url.href);\n\t\t\tshellUrl.searchParams.set("obermon", "1");\n\t\t\treturn shellUrl;\n\t\t});',
    )
    replace_once(
        browser,
        '\t\t\thistory.replaceState(null, "", location.href.split("?")[0]);\n',
        '',
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
