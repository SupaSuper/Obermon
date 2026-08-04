// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/obermon_browser_main_extra_parts.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/obermon/constants.h"
#include "chrome/browser/obermon/obermon_state_service_factory.h"
#include "chrome/browser/obermon/scramjet_engine_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"

namespace obermon {

ObermonBrowserMainExtraParts::ObermonBrowserMainExtraParts() = default;
ObermonBrowserMainExtraParts::~ObermonBrowserMainExtraParts() = default;

void ObermonBrowserMainExtraParts::PostProfileInit(Profile* profile,
                                                   bool is_initial_profile) {
  // Materialize the canonical profile graph before tabs begin attaching. OTR
  // profiles receive their own service instance through the factory policy.
  ObermonStateServiceFactory::GetForProfile(profile);

  // Prewarm without blocking profile initialization. Navigations still defer
  // on EnsureReady(), so a failed prewarm cannot race the first mediated load.
  if (is_initial_profile) {
    ScramjetEngineService::Get()->EnsureReady(base::DoNothing());
  }

  base::FilePath executable_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &executable_dir));
  const base::FilePath product_dir =
      executable_dir.Append(FILE_PATH_LITERAL("obermon"));

  const base::FilePath scramjet_dir =
      product_dir.Append(FILE_PATH_LITERAL("scramjet_extension"));
  const extensions::ExtensionId scramjet_id =
      extensions::ComponentLoader::Get(profile)->AddOrReplace(scramjet_dir);
  CHECK_EQ(scramjet_id, kScramjetExtensionId);

  const base::FilePath theme_dir =
      product_dir.Append(FILE_PATH_LITERAL("theme_extension"));
  const extensions::ExtensionId theme_id =
      extensions::ComponentLoader::Get(profile)->AddOrReplace(theme_dir);
  CHECK_EQ(theme_id, kThemeExtensionId);
  ThemeServiceFactory::GetForProfile(profile)->RevertToExtensionTheme(theme_id);
}

void ObermonBrowserMainExtraParts::PostMainMessageLoopRun() {
  ScramjetEngineService::Get()->Shutdown();
}

}  // namespace obermon
