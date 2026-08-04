// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace obermon {

// Intercepts ordinary top-level HTTP(S) navigations before network dispatch and
// replaces them with the local Scramjet controller navigation. The committed
// entry is subsequently assigned the destination as its virtual URL by
// ScramjetTabHelper.
class ScramjetNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(
      content::NavigationThrottleRegistry& registry);

  ScramjetNavigationThrottle(const ScramjetNavigationThrottle&) = delete;
  ScramjetNavigationThrottle& operator=(const ScramjetNavigationThrottle&) =
      delete;
  ~ScramjetNavigationThrottle() override;

 private:
  explicit ScramjetNavigationThrottle(
      content::NavigationThrottleRegistry& registry);

  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_NAVIGATION_THROTTLE_H_
