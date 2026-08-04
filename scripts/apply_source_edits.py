#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

MARK = "// OBERMON_SOURCE_EDIT"


def insert_after(path: Path, anchor: str, insertion: str) -> None:
    text = path.read_text(encoding="utf-8")
    if insertion.strip() in text:
        return
    if anchor not in text:
        raise RuntimeError(f"Pinned Chromium source changed; anchor missing in {path}: {anchor!r}")
    path.write_text(text.replace(anchor, anchor + insertion, 1), encoding="utf-8")


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"Pinned Chromium source changed; replacement anchor missing in {path}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--chromium", required=True, type=Path)
    args = parser.parse_args()
    root = args.chromium

    main_cc = root / "chrome/browser/chrome_browser_main.cc"
    insert_after(
        main_cc,
        '#include "chrome/browser/chrome_browser_main_extra_parts.h"\n',
        f'#include "chrome/browser/obermon/obermon_browser_main_extra_parts.h"  {MARK}\n',
    )
    insert_after(
        main_cc,
        '  main_parts->AddParts(\n      std::make_unique<ChromeBrowserMainExtraPartsThreadNotifier>(\n          std::move(threads_ready_closure)));\n',
        f'  main_parts->AddParts(\n      std::make_unique<obermon::ObermonBrowserMainExtraParts>());  {MARK}\n',
    )

    observers = root / "chrome/browser/universal_web_contents_observers.cc"
    insert_after(
        observers,
        '#include "chrome/browser/universal_web_contents_observers.h"\n',
        f'#include "chrome/browser/obermon/scramjet_tab_helper.h"  {MARK}\n',
    )
    insert_after(
        observers,
        'void AttachUniversalWebContentsObservers(content::WebContents* web_contents) {\n',
        f'  obermon::ScramjetTabHelper::CreateForWebContents(web_contents);  {MARK}\n',
    )

    ui_util = root / "extensions/browser/ui_util.cc"
    insert_after(
        ui_util,
        '#include "extensions/browser/ui_util.h"\n',
        f'#include "chrome/browser/obermon/constants.h"  {MARK}\n',
    )
    replace_once(
        ui_util,
        'bool ShouldDisplayInExtensionSettings(const Extension& extension) {\n  return ShouldDisplayInExtensionSettings(extension.GetType(),\n                                          extension.location());\n}',
        'bool ShouldDisplayInExtensionSettings(const Extension& extension) {\n  if (extension.id() == obermon::kScramjetExtensionId) {\n    return true;\n  }\n  return ShouldDisplayInExtensionSettings(extension.GetType(),\n                                          extension.location());\n}',
    )

    build = root / "chrome/browser/BUILD.gn"
    insert_after(
        build,
        '    "chrome_browser_main.cc",\n',
        '    "obermon/obermon_browser_main_extra_parts.cc",\n'
        '    "obermon/obermon_browser_main_extra_parts.h",\n'
        '    "obermon/scramjet_engine_service.cc",\n'
        '    "obermon/scramjet_engine_service.h",\n'
        '    "obermon/scramjet_tab_helper.cc",\n'
        '    "obermon/scramjet_tab_helper.h",\n'
        '    "obermon/scramjet_url_mapper.cc",\n'
        '    "obermon/scramjet_url_mapper.h",\n',
    )

    strings = root / "chrome/app/chromium_strings.grd"
    text = strings.read_text(encoding="utf-8")
    if "Obermon" not in text:
        strings.write_text(text.replace("Chromium", "Obermon"), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
