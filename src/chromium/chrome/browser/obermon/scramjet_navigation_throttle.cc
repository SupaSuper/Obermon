// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/obermon/obermon_state_service.h"
#include "chrome/browser/obermon/obermon_state_service_factory.h"
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
      base::WrapUnique(new ScramjetNavigationThrottle(registry)));
}

ScramjetNavigationThrottle::ScramjetNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ScramjetNavigationThrottle::~ScramjetNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ScramjetNavigationThrottle::WillStartRequest() {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
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

  const GURL internal = ScramjetURLMapper::ToInternalURL(destination);
  if (!internal.is_valid()) {
    return CANCEL;
  }

  if (ObermonStateService* state =
          ObermonStateServiceFactory::GetForProfile(profile)) {
    PageMutation mutation;
    mutation.destination_url = destination;
    mutation.internal_url = internal;
    mutation.mediation = PageMediationState::kPreparing;
    mutation.loading = PageLoadingState::kLoading;
    state->UpdatePage(web_contents, mutation);
  }

  ScramjetEngineService* engine = ScramjetEngineService::Get();
  if (engine->is_ready()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OpenInternalURL, web_contents->GetWeakPtr(), internal,
                       handle->GetReferrer(), handle->GetPageTransition(),
                       handle->IsRendererInitiated()));
    return CANCEL_AND_IGNORE;
  }

  engine->EnsureReady(base::BindOnce(
      &ScramjetNavigationThrottle::RedirectAfterBackendReady,
      weak_factory_.GetWeakPtr(), internal, handle->GetReferrer(),
      handle->GetPageTransition(), handle->IsRendererInitiated()));
  return DEFER;
}

void ScramjetNavigationThrottle::RedirectAfterBackendReady(
    GURL internal_url,
    content::Referrer referrer,
    ui::PageTransition transition,
    bool renderer_initiated,
    bool ready) {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!ready) {
    if (profile) {
      if (ObermonStateService* state =
              ObermonStateServiceFactory::GetForProfile(profile)) {
        PageMutation mutation;
        mutation.mediation = PageMediationState::kFailed;
        mutation.loading = PageLoadingState::kIdle;
        state->UpdatePage(web_contents, mutation);
      }
    }
    CancelDeferredNavigation(CANCEL);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenInternalURL, web_contents->GetWeakPtr(), internal_url,
                     referrer, transition, renderer_initiated));
  CancelDeferredNavigation(CANCEL_AND_IGNORE);
}

const char* ScramjetNavigationThrottle::GetNameForLogging() {
  return "ObermonScramjetNavigationThrottle";
}

}  // namespace obermon
