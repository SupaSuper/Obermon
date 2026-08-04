// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_OBERMON_BACKEND_SERVICE_H_
#define CHROME_BROWSER_OBERMON_OBERMON_BACKEND_SERVICE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace obermon {

class MediationBackend;
class ObermonStateService;

// Profile-scoped backend coordinator. It is the single browser-side entry point
// for readiness, navigation preparation, intent hints and request diagnostics.
// This keeps tabs from constructing independent backend policy objects and
// provides the replacement boundary for the future Mojo utility service.
class ObermonBackendService : public KeyedService {
 public:
  enum class IntentStrength {
    kVisible,
    kHover,
    kSelected,
    kCommitted,
  };

  using ReadyCallback = base::OnceCallback<void(bool)>;

  ObermonBackendService(Profile* profile, ObermonStateService* state_service);
  ObermonBackendService(const ObermonBackendService&) = delete;
  ObermonBackendService& operator=(const ObermonBackendService&) = delete;
  ~ObermonBackendService() override;

  void EnsureReady(ReadyCallback callback);
  bool IsReady() const;

  // Returns an authenticated internal navigation URL and updates the canonical
  // page graph to kPreparing. Invalid or unsupported destinations return an
  // empty URL and do not mutate state.
  GURL PrepareNavigation(content::WebContents* web_contents,
                         const GURL& destination);
  void ReportNavigationFailure(content::WebContents* web_contents);

  // Records user intent with short-term duplicate suppression. Strong intent
  // prewarms the backend without creating a renderer or destination request.
  void HintDestination(const GURL& destination, IntentStrength strength);

  Profile* profile() const { return profile_; }
  ObermonStateService* state_service() const { return state_service_; }

  void Shutdown() override;

 private:
  struct IntentRecord {
    IntentStrength strength = IntentStrength::kVisible;
    base::TimeTicks observed_at;
  };

  raw_ptr<Profile> profile_;
  raw_ptr<ObermonStateService> state_service_;
  std::unique_ptr<MediationBackend> backend_;
  base::flat_map<GURL, IntentRecord> recent_intents_;
};

}  // namespace obermon

#endif  // CHROME_BROWSER_OBERMON_OBERMON_BACKEND_SERVICE_H_
