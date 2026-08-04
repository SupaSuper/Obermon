// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_tab_helper.h"

#include "chrome/browser/obermon/obermon_state_service.h"
#include "chrome/browser/obermon/obermon_state_service_factory.h"
#include "chrome/browser/obermon/scramjet_url_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"

namespace obermon {

ScramjetTabHelper::ScramjetTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ScramjetTabHelper>(*web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile) {
    state_service_ = ObermonStateServiceFactory::GetForProfile(profile);
    if (state_service_) {
      state_service_->RegisterPage(web_contents);
    }
  }
}

ScramjetTabHelper::~ScramjetTabHelper() = default;

void ScramjetTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  navigation_starts_[navigation_handle] = base::TimeTicks::Now();
  if (!state_service_) {
    return;
  }

  PageMutation mutation;
  mutation.loading = PageLoadingState::kLoading;
  const GURL& url = navigation_handle->GetURL();
  if (ScramjetURLMapper::IsEligibleDestination(url)) {
    mutation.destination_url = url;
    mutation.mediation = PageMediationState::kPreparing;
  } else if (ScramjetURLMapper::IsInternalURL(url)) {
    mutation.internal_url = url;
  }
  state_service_->UpdatePage(web_contents(), mutation);
}

void ScramjetTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  const GURL& committed_url = navigation_handle->GetURL();
  const auto destination =
      ScramjetURLMapper::DestinationFromInternalURL(committed_url);
  const GURL visible_url = destination.value_or(committed_url);
  RecordNavigationMetadata(navigation_handle, visible_url);

  if (!navigation_handle->HasCommitted()) {
    return;
  }

  if (destination) {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    if (entry && entry->GetVirtualURL() != *destination) {
      entry->SetVirtualURL(*destination);
      web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_URL);
    }

    if (state_service_) {
      PageMutation mutation;
      mutation.destination_url = *destination;
      mutation.internal_url = committed_url;
      mutation.mediation = PageMediationState::kMediated;
      state_service_->UpdatePage(web_contents(), mutation);
    }
    return;
  }

  if (state_service_ && ScramjetURLMapper::IsEligibleDestination(committed_url)) {
    PageMutation mutation;
    mutation.destination_url = committed_url;
    mutation.internal_url = GURL();
    mutation.mediation = PageMediationState::kDirect;
    state_service_->UpdatePage(web_contents(), mutation);
  }
}

void ScramjetTabHelper::DidStartLoading() {
  if (!state_service_) {
    return;
  }
  PageMutation mutation;
  mutation.loading = PageLoadingState::kLoading;
  state_service_->UpdatePage(web_contents(), mutation);
}

void ScramjetTabHelper::DidStopLoading() {
  if (!state_service_) {
    return;
  }
  PageMutation mutation;
  mutation.loading = PageLoadingState::kIdle;
  state_service_->UpdatePage(web_contents(), mutation);
}

void ScramjetTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  if (!state_service_) {
    return;
  }
  PageMutation mutation;
  mutation.lifecycle = visibility == content::Visibility::VISIBLE
                           ? PageLifecycleState::kActive
                           : PageLifecycleState::kBackground;
  state_service_->UpdatePage(web_contents(), mutation);
}

void ScramjetTabHelper::WebContentsDestroyed() {
  navigation_starts_.clear();
  if (state_service_) {
    state_service_->RemovePage(web_contents());
    state_service_ = nullptr;
  }
}

void ScramjetTabHelper::RecordNavigationMetadata(
    content::NavigationHandle* navigation_handle,
    const GURL& visible_url) {
  const auto started = navigation_starts_.find(navigation_handle);
  if (started == navigation_starts_.end()) {
    return;
  }
  const base::TimeDelta duration = base::TimeTicks::Now() - started->second;
  navigation_starts_.erase(started);

  if (!state_service_ || !state_service_->request_capture_enabled()) {
    return;
  }

  RequestMetadata metadata;
  if (const PageState* state = state_service_->GetPageState(web_contents())) {
    metadata.page_id = state->id;
  }
  metadata.url = visible_url;
  metadata.duration = duration;
  if (const net::HttpResponseHeaders* headers =
          navigation_handle->GetResponseHeaders()) {
    metadata.status = headers->response_code();
  }
  state_service_->AppendRequestMetadata(std::move(metadata));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ScramjetTabHelper);

}  // namespace obermon
