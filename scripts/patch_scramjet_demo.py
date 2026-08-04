#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise RuntimeError(
            f"Pinned Scramjet source changed; anchor missing in {path}: {old[:80]!r}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def replace_regex(path: Path, pattern: str, replacement: str) -> None:
    text = path.read_text(encoding="utf-8")
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(
            f"Pinned Scramjet source changed; regex anchor matched {count} times in {path}"
        )
    path.write_text(updated, encoding="utf-8")


def copy_override(source: Path, destination: Path) -> None:
    if not source.exists():
        raise FileNotFoundError(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def patch_request_viewer(path: Path) -> None:
    search_cache = '''\tresponseBodySize?: number;\n};\n\nconst requestSearchCache = new WeakMap<RequestEntry, string>();\nconst getRequestSearchText = (request: RequestEntry): string => {\n\tconst cached = requestSearchCache.get(request);\n\tif (cached !== undefined) return cached;\n\tconst value = [\n\t\trequest.url,\n\t\trequest.method,\n\t\tString(request.status ?? ""),\n\t\trequest.contentType ?? "",\n\t\trequest.destination ?? "",\n\t\trequest.time,\n\t].join(" ").toLowerCase();\n\trequestSearchCache.set(request, value);\n\treturn value;\n};\n\nconst normalizeHeaders'''
    replace_once(
        path,
        '\tresponseBodySize?: number;\n};\n\nconst normalizeHeaders',
        search_cache,
    )

    replace_regex(
        path,
        r'\tconst captureListAnchor = \(\) => \{.*?\n\t\};\n\n\tconst restoreListAnchor',
        '''\tconst captureListAnchor = () => {\n\t\tconst listEl = this.listEl;\n\t\tif (!listEl) return;\n\n\t\tconst stickToBottom = isNearBottom(listEl);\n\t\tif (stickToBottom) {\n\t\t\tthis.listAnchor = { id: null, offset: 0, stickToBottom: true };\n\t\t\treturn;\n\t\t}\n\n\t\tconst scrollTop = listEl.scrollTop;\n\t\tlet firstVisible: HTMLElement | null = null;\n\t\tfor (const child of listEl.children) {\n\t\t\tconst row = child as HTMLElement;\n\t\t\tif (!row.dataset.requestId) continue;\n\t\t\tif (row.offsetTop + row.offsetHeight > scrollTop + 1) {\n\t\t\t\tfirstVisible = row;\n\t\t\t\tbreak;\n\t\t\t}\n\t\t}\n\n\t\tthis.listAnchor = {\n\t\t\tid: firstVisible?.dataset.requestId ?? null,\n\t\t\toffset: firstVisible ? firstVisible.offsetTop - scrollTop : 0,\n\t\t\tstickToBottom: false,\n\t\t};\n\t};\n\n\tconst restoreListAnchor''',
    )

    replace_regex(
        path,
        r'\t\t\t\t\t\t\tconst query = search\.trim\(\)\.toLowerCase\(\);\n\t\t\t\t\t\t\tconst filtered = query\n.*?\n\t\t\t\t\t\t\t\t: requests;',
        '''\t\t\t\t\t\t\tconst query = search.trim().toLowerCase();\n\t\t\t\t\t\t\tconst filtered = query\n\t\t\t\t\t\t\t\t? requests.filter((request) =>\n\t\t\t\t\t\t\t\t\tgetRequestSearchText(request).includes(query)\n\t\t\t\t\t\t\t\t)\n\t\t\t\t\t\t\t\t: requests;''',
    )

    replace_once(
        path,
        '\t.request-row {\n\t\tdisplay: grid;\n',
        '\t.request-row {\n\t\tdisplay: grid;\n\t\tcontent-visibility: auto;\n\t\tcontain: layout paint style;\n\t\tcontain-intrinsic-size: auto 58px;\n',
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scramjet", required=True, type=Path)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    overrides = repo_root / "src" / "scramjet-overrides"
    demo_source = args.scramjet / "packages" / "demo" / "src"
    utils_source = args.scramjet / "packages" / "utils" / "src"

    copy_override(overrides / "index.tsx", demo_source / "index.tsx")
    copy_override(
        overrides / "BrowserView.tsx",
        demo_source / "pages" / "BrowserView.tsx",
    )
    copy_override(
        overrides / "link-handler-plugin.ts",
        utils_source / "link-handler-plugin.ts",
    )

    patch_request_viewer(demo_source / "pages" / "RequestViewer.tsx")
    replace_once(
        demo_source / "pages" / "SettingsPage.tsx",
        'controller.setTransport(getTransport());',
        'controller.setTransport(await getTransport());',
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
