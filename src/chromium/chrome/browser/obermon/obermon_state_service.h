// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_OBERMON_STATE_SERVICE_H_
#define CHROME_BROWSER_OBERMON_OBERMON_STATE_SERVICE_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace obermon {

using PageId = uint64_t;
using PageStateFieldMask = uint32_t;

// These values deliberately describe Obermon's product lifecycle rather than
// mirroring one renderer enum. A page can remain warm even after a renderer is
// frozen, and can be prewarmed before a renderer exists.
enum class PageLifecycleState {
  kCold,
  kPrewarmed,
  kActive,
  kBackground,
  kFrozen,
  kDiscarded,
};

enum class PageMediationState {
  kDirect,
  kPreparing,
  kMediated,
  kFailed,
};

enum class PageLoadingState {
  kIdle,
  kLoading,
};

enum PageStateField : PageStateFieldMask {
  kPageStateNone = 0,
  kPageStateDestinationUrl = 1u << 0,
  kPageStateInternalUrl = 1u << 1,
  kPageStateLifecycle = 1u << 2,
  kPageStateMediation = 1u << 3,
  kPageStateLoading = 1u << 4,
  kPageStateAudible = 1u << 5,
  kPageStatePinned = 1u << 6,
  kPageStateUnsavedForm = 1u << 7,
  kPageStateAll = (1u << 8) - 1,
};

struct PageState {
  PageId id = 0;
  uint64_t revision = 0;
  GURL destination_url;
  GURL internal_url;
  PageLifecycleState lifecycle = PageLifecycleState::kCold;
  PageMediationState mediation = PageMediationState::kDirect;
  PageLoadingState loading = PageLoadingState::kIdle;
  bool audible = false;
  bool pinned = false;
  bool has_unsaved_form = false;
};

// A mutation is sparse by design. Callers only specify fields they actually
// observed changing, and the service suppresses assignments that are equal to
// the canonical state.
struct PageMutation {
  std::optional<GURL> destination_url;
  std::optional<GURL> internal_url;
  std::optional<PageLifecycleState> lifecycle;
  std::optional<PageMediationState> mediation;
  std::optional<PageLoadingState> loading;
  std::optional<bool> audible;
  std::optional<bool> pinned;
  std::optional<bool> has_unsaved_form;
};

struct PageStateDelta {
  PageState snapshot;
  PageStateFieldMask changed_fields = kPageStateNone;
};

// Request diagnostics are metadata-only on the hot path. Bodies are never
// copied into this record; explicit bounded body capture belongs to a separate
// diagnostic path.
struct RequestMetadata {
  uint64_t request_id = 0;
  PageId page_id = 0;
  GURL url;
  int status = 0;
  base::TimeDelta duration;
  uint64_t encoded_bytes = 0;
  uint64_t decoded_bytes = 0;
  base::TimeTicks recorded_at;
};

class ObermonStateService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPageStateChanged(const PageStateDelta& delta) = 0;
    virtual void OnPageRemoved(PageId page_id) = 0;
  };

  class RequestObserver : public base::CheckedObserver {
   public:
    virtual void OnRequestMetadata(const RequestMetadata& metadata) = 0;
  };

  explicit ObermonStateService(Profile* profile);
  ObermonStateService(const ObermonStateService&) = delete;
  ObermonStateService& operator=(const ObermonStateService&) = delete;
  ~ObermonStateService() override;

  PageId RegisterPage(content::WebContents* web_contents);
  void RemovePage(content::WebContents* web_contents);
  PageStateFieldMask UpdatePage(content::WebContents* web_contents,
                                const PageMutation& mutation);

  const PageState* GetPageState(content::WebContents* web_contents) const;
  std::vector<PageState> GetPageSnapshot() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetRequestCaptureEnabled(bool enabled);
  bool request_capture_enabled() const { return request_capture_enabled_; }
  void AppendRequestMetadata(RequestMetadata metadata);
  std::vector<RequestMetadata> GetRecentRequests(size_t limit) const;
  void AddRequestObserver(RequestObserver* observer);
  void RemoveRequestObserver(RequestObserver* observer);

  Profile* profile() const { return profile_; }

  // KeyedService:
  void Shutdown() override;

 private:
  using PageMap =
      base::flat_map<raw_ptr<content::WebContents>, PageState>;

  void SchedulePageDeltaFlush();
  void FlushPageDeltas();

  raw_ptr<Profile> profile_;
  PageId next_page_id_ = 1;
  uint64_t next_request_id_ = 1;
  PageMap pages_;
  base::flat_map<raw_ptr<content::WebContents>, PageStateFieldMask>
      pending_page_changes_;
  bool page_flush_scheduled_ = false;

  base::ObserverList<Observer> observers_;
  base::ObserverList<RequestObserver> request_observers_;

  bool request_capture_enabled_ = false;
  base::circular_deque<RequestMetadata> request_metadata_;

  base::WeakPtrFactory<ObermonStateService> weak_factory_{this};
};

}  // namespace obermon

#endif  // CHROME_BROWSER_OBERMON_OBERMON_STATE_SERVICE_H_
