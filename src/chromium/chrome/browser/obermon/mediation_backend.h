// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_MEDIATION_BACKEND_H_
#define CHROME_BROWSER_OBERMON_MEDIATION_BACKEND_H_

#include <memory>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace obermon {

// Stable browser-process seam between navigation policy and the active
// mediation implementation. The current implementation is loopback-based; the
// planned Mojo utility service can replace it without changing tab or state
// management code.
class MediationBackend {
 public:
  enum class State {
    kStopped,
    kStarting,
    kReady,
    kFailed,
    kStopping,
  };

  using ReadyCallback = base::OnceCallback<void(bool)>;

  virtual ~MediationBackend() = default;

  virtual void EnsureReady(ReadyCallback callback) = 0;
  virtual State state() const = 0;
  virtual bool IsReady() const = 0;
  virtual GURL CreateNavigationURL(const GURL& destination) = 0;
  virtual void Shutdown() = 0;
};

std::unique_ptr<MediationBackend> CreateLoopbackMediationBackend();

}  // namespace obermon

#endif  // CHROME_BROWSER_OBERMON_MEDIATION_BACKEND_H_
