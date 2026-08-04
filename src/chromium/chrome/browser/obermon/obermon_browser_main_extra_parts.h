// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_OBERMON_BROWSER_MAIN_EXTRA_PARTS_H_
#define CHROME_BROWSER_OBERMON_OBERMON_BROWSER_MAIN_EXTRA_PARTS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace obermon {

class ObermonBrowserMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  ObermonBrowserMainExtraParts();
  ~ObermonBrowserMainExtraParts() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PostMainMessageLoopRun() override;
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_OBERMON_BROWSER_MAIN_EXTRA_PARTS_H_
