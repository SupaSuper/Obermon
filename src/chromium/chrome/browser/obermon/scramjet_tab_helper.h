// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_TAB_HELPER_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_TAB_HELPER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

namespace obermon {

class ObermonStateService;

class ScramjetTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ScramjetTabHelper> {
 public:
  ScramjetTabHelper(const ScramjetTabHelper&) = delete;
  ScramjetTabHelper& operator=(const ScramjetTabHelper&) = delete;
  ~ScramjetTabHelper() override;

  void DidStartNavigation(content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

 private:
  explicit ScramjetTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ScramjetTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void RecordNavigationMetadata(content::NavigationHandle* navigation_handle,
                                const GURL& visible_url);

  raw_ptr<ObermonStateService> state_service_ = nullptr;
  base::flat_map<raw_ptr<content::NavigationHandle>, base::TimeTicks>
      navigation_starts_;
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_TAB_HELPER_H_
