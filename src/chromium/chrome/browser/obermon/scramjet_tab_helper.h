// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_TAB_HELPER_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

namespace obermon {

class ScramjetTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ScramjetTabHelper> {
 public:
  ScramjetTabHelper(const ScramjetTabHelper&) = delete;
  ScramjetTabHelper& operator=(const ScramjetTabHelper&) = delete;
  ~ScramjetTabHelper() override;

  void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;

 private:
  explicit ScramjetTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ScramjetTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_TAB_HELPER_H_
