// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/obermon_backend_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "chrome/browser/obermon/mediation_backend.h"
#include "chrome/browser/obermon/obermon_state_service.h"
#include "chrome/browser/obermon/scramjet_url_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace obermon {
namespace {

constexpr base::TimeDelta kIntentDeduplicationWindow = base::Milliseconds(250);
constexpr base::TimeDelta kIntentRetention = base::Seconds(10);

}  // namespace

ObermonBackendService::ObermonBackendService(
    Profile* profile,
    ObermonStateService* state_service)
    : profile_(profile),
      state_service_(state_service),
      backend_(CreateLoopbackMediationBackend()),
      transport_partition_(
          base::Uuid::GenerateRandomV4().AsLowercaseString()) {}

ObermonBackendService::~ObermonBackendService() = default;

void ObermonBackendService::EnsureReady(ReadyCallback callback) {
  backend_->EnsureReady(std::move(callback));
}

bool ObermonBackendService::IsReady() const {
  return backend_->IsReady();
}

GURL ObermonBackendService::PrepareNavigation(
    content::WebContents* web_contents,
    const GURL& destination) {
  if (!web_contents || !ScramjetURLMapper::IsEligibleDestination(destination)) {
    return GURL();
  }

  const GURL internal =
      backend_->CreateNavigationURL(destination, transport_partition_);
  if (!internal.is_valid()) {
    return GURL();
  }

  if (state_service_) {
    PageMutation mutation;
    mutation.destination_url = destination;
    mutation.internal_url = internal;
    mutation.mediation = PageMediationState::kPreparing;
    mutation.loading = PageLoadingState::kLoading;
    state_service_->UpdatePage(web_contents, mutation);
  }
  return internal;
}

void ObermonBackendService::ReportNavigationFailure(
    content::WebContents* web_contents) {
  if (!state_service_ || !web_contents) {
    return;
  }
  PageMutation mutation;
  mutation.mediation = PageMediationState::kFailed;
  mutation.loading = PageLoadingState::kIdle;
  state_service_->UpdatePage(web_contents, mutation);
}

void ObermonBackendService::HintDestination(const GURL& destination,
                                            IntentStrength strength) {
  if (!ScramjetURLMapper::IsEligibleDestination(destination)) {
    return;
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  for (auto it = recent_intents_.begin(); it != recent_intents_.end();) {
    if (now - it->second.observed_at > kIntentRetention) {
      it = recent_intents_.erase(it);
    } else {
      ++it;
    }
  }

  const auto existing = recent_intents_.find(destination);
  if (existing != recent_intents_.end() &&
      existing->second.strength >= strength &&
      now - existing->second.observed_at < kIntentDeduplicationWindow) {
    return;
  }
  recent_intents_[destination] = IntentRecord{strength, now};

  // Hover remains a cheap signal. Selection and commit indicate enough intent
  // to start the shared mediation backend, but do not create a renderer or send
  // a destination request.
  if (strength >= IntentStrength::kSelected) {
    EnsureReady(base::BindOnce([](bool) {}));
  }
}

void ObermonBackendService::Shutdown() {
  recent_intents_.clear();
  if (backend_) {
    backend_->Shutdown();
    backend_.reset();
  }
  state_service_ = nullptr;
  profile_ = nullptr;
}

}  // namespace obermon
