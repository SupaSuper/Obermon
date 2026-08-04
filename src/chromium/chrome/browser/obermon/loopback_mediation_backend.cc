// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/mediation_backend.h"

#include <memory>

#include "chrome/browser/obermon/scramjet_engine_service.h"
#include "chrome/browser/obermon/scramjet_url_mapper.h"

namespace obermon {
namespace {

class LoopbackMediationBackend final : public MediationBackend {
 public:
  LoopbackMediationBackend() = default;
  ~LoopbackMediationBackend() override = default;

  void EnsureReady(ReadyCallback callback) override {
    ScramjetEngineService::Get()->EnsureReady(std::move(callback));
  }

  State state() const override {
    switch (ScramjetEngineService::Get()->state()) {
      case ScramjetEngineService::State::kStopped:
        return State::kStopped;
      case ScramjetEngineService::State::kStarting:
        return State::kStarting;
      case ScramjetEngineService::State::kReady:
        return State::kReady;
      case ScramjetEngineService::State::kFailed:
        return State::kFailed;
      case ScramjetEngineService::State::kStopping:
        return State::kStopping;
    }
  }

  bool IsReady() const override {
    return ScramjetEngineService::Get()->is_ready();
  }

  GURL CreateNavigationURL(const GURL& destination) override {
    return ScramjetURLMapper::ToInternalURL(destination);
  }

  void Shutdown() override {
    // The transitional loopback process is shared by profile coordinators and
    // is shut down once by ObermonBrowserMainExtraParts.
  }
};

}  // namespace

std::unique_ptr<MediationBackend> CreateLoopbackMediationBackend() {
  return std::make_unique<LoopbackMediationBackend>();
}

}  // namespace obermon
