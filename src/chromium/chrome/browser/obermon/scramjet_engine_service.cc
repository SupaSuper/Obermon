// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_engine_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/uuid.h"
#include "build/build_config.h"

namespace obermon {
namespace {

constexpr base::TimeDelta kReadyPollInterval = base::Milliseconds(25);
constexpr base::TimeDelta kStartupTimeout = base::Seconds(5);

}  // namespace

ScramjetEngineService::ScramjetEngineService() = default;
ScramjetEngineService::~ScramjetEngineService() = default;

ScramjetEngineService* ScramjetEngineService::Get() {
  static base::NoDestructor<ScramjetEngineService> instance;
  return instance.get();
}

void ScramjetEngineService::EnsureReady(ReadyCallback callback) {
  if (is_ready()) {
    std::move(callback).Run(true);
    return;
  }

  if (state_ == State::kReady && !is_running()) {
    state_ = State::kStopped;
  }

  ready_callbacks_.push_back(std::move(callback));
  if (state_ == State::kStarting) {
    return;
  }

  if (!LaunchProcess()) {
    CompleteStartup(false);
    return;
  }

  state_ = State::kStarting;
  startup_deadline_ = base::TimeTicks::Now() + kStartupTimeout;
  ready_poll_timer_.Start(
      FROM_HERE, kReadyPollInterval,
      base::BindRepeating(&ScramjetEngineService::PollReadiness,
                          weak_factory_.GetWeakPtr()));
}

bool ScramjetEngineService::LaunchProcess() {
  if (is_running()) {
    return true;
  }

  base::FilePath executable_dir;
  base::FilePath temp_dir;
  if (!base::PathService::Get(base::DIR_EXE, &executable_dir) ||
      !base::GetTempDir(&temp_dir)) {
    return false;
  }

  const base::FilePath engine =
      executable_dir.Append(FILE_PATH_LITERAL("obermon"))
          .Append(FILE_PATH_LITERAL("scramjet-engine.exe"));
  if (!base::PathExists(engine)) {
    return false;
  }

  ResetReadyMarker();
  ready_file_ = temp_dir.AppendASCII(
      "obermon-scramjet-" +
      base::Uuid::GenerateRandomV4().AsLowercaseString() + ".ready");
  base::DeleteFile(ready_file_);

  base::CommandLine command(engine);
  command.AppendSwitch("server");
  command.AppendSwitchPath("ready-file", ready_file_);
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  process_ = base::LaunchProcess(command, options);
  return process_.IsValid();
}

void ScramjetEngineService::PollReadiness() {
  if (!is_running()) {
    CompleteStartup(false);
    return;
  }
  if (!ready_file_.empty() && base::PathExists(ready_file_)) {
    CompleteStartup(true);
    return;
  }
  if (base::TimeTicks::Now() >= startup_deadline_) {
    CompleteStartup(false);
  }
}

void ScramjetEngineService::CompleteStartup(bool ready) {
  ready_poll_timer_.Stop();
  if (ready) {
    state_ = State::kReady;
  } else {
    state_ = State::kFailed;
    if (process_.IsValid()) {
      process_.Terminate(1, false);
      process_.Close();
    }
  }
  ResetReadyMarker();
  RunReadyCallbacks(ready);
}

void ScramjetEngineService::RunReadyCallbacks(bool ready) {
  std::vector<ReadyCallback> callbacks = std::move(ready_callbacks_);
  ready_callbacks_.clear();
  for (ReadyCallback& callback : callbacks) {
    if (callback) {
      std::move(callback).Run(ready);
    }
  }
}

void ScramjetEngineService::Shutdown() {
  state_ = State::kStopping;
  weak_factory_.InvalidateWeakPtrs();
  ready_poll_timer_.Stop();
  RunReadyCallbacks(false);
  if (process_.IsValid()) {
    process_.Terminate(0, false);
    process_.Close();
  }
  ResetReadyMarker();
  state_ = State::kStopped;
}

bool ScramjetEngineService::is_running() const {
  return process_.IsValid() && process_.IsRunning();
}

bool ScramjetEngineService::is_ready() const {
  return state_ == State::kReady && is_running();
}

void ScramjetEngineService::ResetReadyMarker() {
  if (!ready_file_.empty()) {
    base::DeleteFile(ready_file_);
    ready_file_.clear();
  }
}

}  // namespace obermon
