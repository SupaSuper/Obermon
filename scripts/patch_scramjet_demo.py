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


def patch_controller(path: Path) -> None:
    replace_once(
        path,
        'function makeId(): string {\n\treturn Math.random().toString(36).substring(2, 10);\n}',
        'function makeId(): string {\n\treturn crypto.randomUUID().replaceAll("-", "");\n}',
    )
    replace_once(
        path,
        '\tprivate wasmPayload: string | null = null;\n',
        '\tprivate virtualWasmPromise: Promise<string> | null = null;\n',
    )
    replace_regex(
        path,
        r'\t\t\t\tif \(path === frame\.prefix \+ this\.config\.virtualWasmPath\) \{.*?\n\t\t\t\t\}\n\n\t\t\t\tconst sjheaders',
        '''\t\t\t\tif (path === frame.prefix + this.config.virtualWasmPath) {\n\t\t\t\t\tthis.virtualWasmPromise ??= fetch(`${this.config.wasmPath}.js`).then(\n\t\t\t\t\t\tasync (response) => {\n\t\t\t\t\t\t\tif (!response.ok) {\n\t\t\t\t\t\t\t\tthrow new Error(`Failed to load prebuilt WASM payload: ${response.status}`);\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\treturn response.text();\n\t\t\t\t\t\t}\n\t\t\t\t\t);\n\t\t\t\t\treturn [\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\tbody: await this.virtualWasmPromise,\n\t\t\t\t\t\t\tstatus: 200,\n\t\t\t\t\t\t\tstatusText: "OK",\n\t\t\t\t\t\t\theaders: [["Content-Type", "application/javascript"]],\n\t\t\t\t\t\t},\n\t\t\t\t\t\t[],\n\t\t\t\t\t];\n\t\t\t\t}\n\n\t\t\t\tconst sjheaders''',
    )


def patch_http_cache(path: Path) -> None:
    replace_once(
        path,
        'const STORED_AT_HEADER = "x-sj-cached-at";\n',
        'const STORED_AT_HEADER = "x-sj-cached-at";\n\n'
        '// Known bodies above this size bypass the proxy cache so large media and\n'
        '// downloads remain fully streaming. Unknown-length responses still use a\n'
        '// tee, which avoids blocking the rewrite path on Cache Storage.\n'
        'const MAX_CACHEABLE_BODY_BYTES = 8 * 1024 * 1024;\n',
    )
    replace_once(
        path,
        'function buildStorableResponse(\n\tbody: ArrayBuffer | null,\n',
        'function buildStorableResponse(\n\tbody: BodyInit | null,\n',
    )
    replace_regex(
        path,
        r'\t\t\t// Drain the stream once and rebuild the BareResponse around the\n.*?\n\t\t\t}\n\t\t\}\);',
        '''\t\t\tconst contentLength = Number.parseInt(\n\t\t\t\theaders.get("content-length") ?? "",\n\t\t\t\t10\n\t\t\t);\n\t\t\tif (\n\t\t\t\tNumber.isFinite(contentLength) &&\n\t\t\t\tcontentLength > MAX_CACHEABLE_BODY_BYTES\n\t\t\t) {\n\t\t\t\treturn;\n\t\t\t}\n\n\t\t\tconst original = props.response;\n\t\t\tconst nullBody = NULL_BODY_STATUSES.has(original.status);\n\t\t\tlet pipelineBody: ReadableStream<Uint8Array> | null = null;\n\t\t\tlet cacheBody: ReadableStream<Uint8Array> | null = null;\n\t\t\tif (!nullBody && original.body) {\n\t\t\t\t[pipelineBody, cacheBody] = original.body.tee();\n\t\t\t}\n\n\t\t\tconst replacement = BareResponse.fromNativeResponse(\n\t\t\t\tnew Response(pipelineBody, {\n\t\t\t\t\tstatus: original.status,\n\t\t\t\t\tstatusText: original.statusText,\n\t\t\t\t\theaders,\n\t\t\t\t})\n\t\t\t);\n\t\t\treplacement.url = original.url;\n\t\t\treplacement.redirected = original.redirected;\n\t\t\treplacement.rawHeaders = [...original.rawHeaders];\n\t\t\tprops.response = replacement;\n\n\t\t\tconst cacheKey = buildCacheKeyRequest(\n\t\t\t\tctx.parsed.url.href,\n\t\t\t\treq.initialHeaders\n\t\t\t);\n\t\t\tconst toStore = buildStorableResponse(\n\t\t\t\tcacheBody,\n\t\t\t\toriginal.status,\n\t\t\t\toriginal.statusText,\n\t\t\t\toriginal.rawHeaders\n\t\t\t);\n\n\t\t\tvoid this.openCache()\n\t\t\t\t.then((cache) => cache.put(cacheKey, toStore))\n\t\t\t\t.catch((error) => {\n\t\t\t\t\tconsole.warn("[scramjet-http-cache] cache.put failed:", error);\n\t\t\t\t});\n\t\t});''',
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scramjet", required=True, type=Path)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    overrides = repo_root / "src" / "scramjet-overrides"
    demo_source = args.scramjet / "packages" / "demo" / "src"
    controller_source = args.scramjet / "packages" / "controller" / "src"
    utils_source = args.scramjet / "packages" / "utils" / "src"

    for source_name, destination_name in (
        ("index.tsx", "index.tsx"),
        ("shared-transport-client.ts", "shared-transport-client.ts"),
        ("shared-transport-worker.ts", "shared-transport-worker.ts"),
    ):
        copy_override(overrides / source_name, demo_source / destination_name)
    copy_override(
        overrides / "BrowserView.tsx",
        demo_source / "pages" / "BrowserView.tsx",
    )
    copy_override(
        overrides / "link-handler-plugin.ts",
        utils_source / "link-handler-plugin.ts",
    )

    patch_request_viewer(demo_source / "pages" / "RequestViewer.tsx")
    patch_controller(controller_source / "index.ts")
    patch_http_cache(utils_source / "http-cache-plugin.ts")
    replace_once(
        demo_source / "pages" / "SettingsPage.tsx",
        'controller.setTransport(getTransport());',
        'controller.setTransport(await getTransport());',
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
