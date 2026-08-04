// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace obermon {

// Intercepts ordinary top-level HTTP(S) navigations before network dispatch and
// replaces them with the local Scramjet controller navigation. Startup is
// asynchronous: the original request remains deferred until the mediation
// backend reports that all listeners are ready.
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

  void RedirectAfterBackendReady(GURL internal_url,
                                 content::Referrer referrer,
                                 ui::PageTransition transition,
                                 bool renderer_initiated,
                                 bool ready);

  base::WeakPtrFactory<ScramjetNavigationThrottle> weak_factory_{this};
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_NAVIGATION_THROTTLE_H_
