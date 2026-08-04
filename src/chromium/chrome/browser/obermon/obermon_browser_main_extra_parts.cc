// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/obermon_browser_main_extra_parts.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/obermon/constants.h"
#include "chrome/browser/obermon/scramjet_engine_service.h"
#include "chrome/browser/profiles/profile.h"

namespace obermon {

ObermonBrowserMainExtraParts::ObermonBrowserMainExtraParts() = default;
ObermonBrowserMainExtraParts::~ObermonBrowserMainExtraParts() = default;

void ObermonBrowserMainExtraParts::PostProfileInit(Profile* profile,
                                                   bool is_initial_profile) {
  if (is_initial_profile) {
    CHECK(ScramjetEngineService::Get()->Start());
  }

  base::FilePath executable_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &executable_dir));
  const base::FilePath extension_dir =
      executable_dir.Append(FILE_PATH_LITERAL("obermon"))
          .Append(FILE_PATH_LITERAL("scramjet_extension"));
  const extensions::ExtensionId id =
      extensions::ComponentLoader::Get(profile)->AddOrReplace(extension_dir);
  CHECK_EQ(id, kScramjetExtensionId);
}

void ObermonBrowserMainExtraParts::PostMainMessageLoopRun() {
  ScramjetEngineService::Get()->Shutdown();
}

}  // namespace obermon
