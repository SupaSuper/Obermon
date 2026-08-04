// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_navigation_throttle.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/obermon/pref_names.h"
#include "chrome/browser/obermon/scramjet_engine_service.h"
#include "chrome/browser/obermon/scramjet_url_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/open_url_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

namespace obermon {
namespace {

void OpenInternalURL(base::WeakPtr<content::WebContents> web_contents,
                     const GURL& internal_url,
                     const content::Referrer& referrer,
                     ui::PageTransition transition,
                     bool renderer_initiated) {
  if (!web_contents) {
    return;
  }
  content::OpenURLParams params(internal_url, referrer,
                                WindowOpenDisposition::CURRENT_TAB, transition,
                                renderer_initiated);
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
}

}  // namespace

// static
void ScramjetNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() || handle.IsSameDocument()) {
    return;
  }
  registry.AddThrottle(
      std::make_unique<ScramjetNavigationThrottle>(registry));
}

ScramjetNavigationThrottle::ScramjetNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ScramjetNavigationThrottle::~ScramjetNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ScramjetNavigationThrottle::WillStartRequest() {
  content::NavigationHandle* handle = navigation_handle();
  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  if (!profile || !profile->GetPrefs()->GetBoolean(prefs::kScramjetEnabled)) {
    return PROCEED;
  }

  // Scramjet owns subsequent form submissions and fetches inside its frame. An
  // initial non-GET browser navigation cannot be represented by the controller's
  // destination URL alone, so let it proceed directly rather than dropping data.
  if (handle->GetMethod() != "GET") {
    return PROCEED;
  }

  const GURL destination = handle->GetURL();
  if (!ScramjetURLMapper::IsEligibleDestination(destination)) {
    return PROCEED;
  }

  if (!ScramjetEngineService::Get()->is_running() &&
      !ScramjetEngineService::Get()->Start()) {
    return CANCEL;
  }

  const GURL internal = ScramjetURLMapper::ToInternalURL(destination);
  if (!internal.is_valid()) {
    return CANCEL;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenInternalURL,
                     handle->GetWebContents()->GetWeakPtr(), internal,
                     handle->GetReferrer(), handle->GetPageTransition(),
                     handle->IsRendererInitiated()));
  return CANCEL_AND_IGNORE;
}

const char* ScramjetNavigationThrottle::GetNameForLogging() {
  return "ObermonScramjetNavigationThrottle";
}

}  // namespace obermon
