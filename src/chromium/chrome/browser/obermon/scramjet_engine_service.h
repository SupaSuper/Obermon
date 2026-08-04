// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_ENGINE_SERVICE_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_ENGINE_SERVICE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace obermon {

// Owns the transitional loopback engine process. Unlike the initial
// implementation, process creation is not treated as readiness: navigation is
// released only after both HTTP and Wisp listeners have published an atomic
// ready marker.
class ScramjetEngineService {
 public:
  enum class State {
    kStopped,
    kStarting,
    kReady,
    kFailed,
    kStopping,
  };

  using ReadyCallback = base::OnceCallback<void(bool)>;

  static ScramjetEngineService* Get();
  ScramjetEngineService(const ScramjetEngineService&) = delete;
  ScramjetEngineService& operator=(const ScramjetEngineService&) = delete;

  void EnsureReady(ReadyCallback callback);
  void Shutdown();

  State state() const { return state_; }
  bool is_running() const;
  bool is_ready() const;

 private:
  friend class base::NoDestructor<ScramjetEngineService>;
  ScramjetEngineService();
  ~ScramjetEngineService();

  bool LaunchProcess();
  void PollReadiness();
  void CompleteStartup(bool ready);
  void RunReadyCallbacks(bool ready);
  void ResetReadyMarker();

  State state_ = State::kStopped;
  base::Process process_;
  base::FilePath ready_file_;
  base::TimeTicks startup_deadline_;
  base::RepeatingTimer ready_poll_timer_;
  std::vector<ReadyCallback> ready_callbacks_;
  base::WeakPtrFactory<ScramjetEngineService> weak_factory_{this};
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_ENGINE_SERVICE_H_
