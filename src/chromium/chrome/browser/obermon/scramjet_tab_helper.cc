// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_tab_helper.h"

#include "chrome/browser/obermon/scramjet_url_mapper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace obermon {

ScramjetTabHelper::ScramjetTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ScramjetTabHelper>(*web_contents) {}

ScramjetTabHelper::~ScramjetTabHelper() = default;

void ScramjetTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  const auto destination =
      ScramjetURLMapper::DestinationFromInternalURL(navigation_handle->GetURL());
  if (!destination) {
    return;
  }
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (entry) {
    entry->SetVirtualURL(*destination);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ScramjetTabHelper);

}  // namespace obermon
