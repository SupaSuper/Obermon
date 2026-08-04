// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/obermon_state_service.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace obermon {
namespace {

constexpr size_t kMaximumRequestMetadataRecords = 1024;

template <typename T>
bool AssignIfChanged(T& destination, const std::optional<T>& source) {
  if (!source || destination == *source) {
    return false;
  }
  destination = *source;
  return true;
}

}  // namespace

ObermonStateService::ObermonStateService(Profile* profile) : profile_(profile) {}

ObermonStateService::~ObermonStateService() = default;

PageId ObermonStateService::RegisterPage(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  const auto existing = pages_.find(web_contents);
  if (existing != pages_.end()) {
    return existing->second.id;
  }

  PageState initial;
  initial.id = next_page_id_++;
  initial.lifecycle =
      web_contents->GetVisibility() == content::Visibility::VISIBLE
          ? PageLifecycleState::kActive
          : PageLifecycleState::kBackground;
  const PageId page_id = initial.id;
  pages_.emplace(web_contents, std::move(initial));
  pending_page_changes_[web_contents] = kPageStateAll;
  SchedulePageDeltaFlush();
  return page_id;
}

void ObermonStateService::RemovePage(content::WebContents* web_contents) {
  const auto it = pages_.find(web_contents);
  if (it == pages_.end()) {
    return;
  }

  const PageId page_id = it->second.id;
  pending_page_changes_.erase(web_contents);
  pages_.erase(it);
  for (Observer& observer : observers_) {
    observer.OnPageRemoved(page_id);
  }
}

PageStateFieldMask ObermonStateService::UpdatePage(
    content::WebContents* web_contents,
    const PageMutation& mutation) {
  CHECK(web_contents);
  auto it = pages_.find(web_contents);
  if (it == pages_.end()) {
    RegisterPage(web_contents);
    it = pages_.find(web_contents);
  }

  PageState& state = it->second;
  PageStateFieldMask changed = kPageStateNone;
  if (AssignIfChanged(state.destination_url, mutation.destination_url)) {
    changed |= kPageStateDestinationUrl;
  }
  if (AssignIfChanged(state.internal_url, mutation.internal_url)) {
    changed |= kPageStateInternalUrl;
  }
  if (AssignIfChanged(state.lifecycle, mutation.lifecycle)) {
    changed |= kPageStateLifecycle;
  }
  if (AssignIfChanged(state.mediation, mutation.mediation)) {
    changed |= kPageStateMediation;
  }
  if (AssignIfChanged(state.loading, mutation.loading)) {
    changed |= kPageStateLoading;
  }
  if (AssignIfChanged(state.audible, mutation.audible)) {
    changed |= kPageStateAudible;
  }
  if (AssignIfChanged(state.pinned, mutation.pinned)) {
    changed |= kPageStatePinned;
  }
  if (AssignIfChanged(state.has_unsaved_form, mutation.has_unsaved_form)) {
    changed |= kPageStateUnsavedForm;
  }

  if (changed == kPageStateNone) {
    return changed;
  }

  ++state.revision;
  pending_page_changes_[web_contents] |= changed;
  SchedulePageDeltaFlush();
  return changed;
}

const PageState* ObermonStateService::GetPageState(
    content::WebContents* web_contents) const {
  const auto it = pages_.find(web_contents);
  return it == pages_.end() ? nullptr : &it->second;
}

std::vector<PageState> ObermonStateService::GetPageSnapshot() const {
  std::vector<PageState> snapshot;
  snapshot.reserve(pages_.size());
  for (const auto& entry : pages_) {
    snapshot.push_back(entry.second);
  }
  return snapshot;
}

void ObermonStateService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ObermonStateService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ObermonStateService::SetRequestCaptureEnabled(bool enabled) {
  if (request_capture_enabled_ == enabled) {
    return;
  }
  request_capture_enabled_ = enabled;
  if (!enabled) {
    request_metadata_.clear();
  }
}

void ObermonStateService::AppendRequestMetadata(RequestMetadata metadata) {
  if (!request_capture_enabled_) {
    return;
  }

  if (metadata.request_id == 0) {
    metadata.request_id = next_request_id_++;
  }
  if (metadata.recorded_at.is_null()) {
    metadata.recorded_at = base::TimeTicks::Now();
  }

  request_metadata_.push_back(metadata);
  while (request_metadata_.size() > kMaximumRequestMetadataRecords) {
    request_metadata_.pop_front();
  }
  for (RequestObserver& observer : request_observers_) {
    observer.OnRequestMetadata(metadata);
  }
}

std::vector<RequestMetadata> ObermonStateService::GetRecentRequests(
    size_t limit) const {
  const size_t count = std::min(limit, request_metadata_.size());
  std::vector<RequestMetadata> snapshot;
  snapshot.reserve(count);
  const auto first =
      std::next(request_metadata_.begin(), request_metadata_.size() - count);
  snapshot.insert(snapshot.end(), first, request_metadata_.end());
  return snapshot;
}

void ObermonStateService::AddRequestObserver(RequestObserver* observer) {
  request_observers_.AddObserver(observer);
}

void ObermonStateService::RemoveRequestObserver(RequestObserver* observer) {
  request_observers_.RemoveObserver(observer);
}

void ObermonStateService::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
  page_flush_scheduled_ = false;
  pending_page_changes_.clear();
  pages_.clear();
  request_metadata_.clear();
  profile_ = nullptr;
}

void ObermonStateService::SchedulePageDeltaFlush() {
  if (page_flush_scheduled_) {
    return;
  }
  page_flush_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ObermonStateService::FlushPageDeltas,
                     weak_factory_.GetWeakPtr()));
}

void ObermonStateService::FlushPageDeltas() {
  page_flush_scheduled_ = false;

  std::vector<PageStateDelta> deltas;
  deltas.reserve(pending_page_changes_.size());
  for (const auto& [web_contents, fields] : pending_page_changes_) {
    const auto state = pages_.find(web_contents);
    if (state == pages_.end()) {
      continue;
    }
    deltas.push_back(PageStateDelta{state->second, fields});
  }
  pending_page_changes_.clear();

  for (const PageStateDelta& delta : deltas) {
    for (Observer& observer : observers_) {
      observer.OnPageStateChanged(delta);
    }
  }
}

}  // namespace obermon
