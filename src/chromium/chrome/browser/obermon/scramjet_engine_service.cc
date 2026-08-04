// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_engine_service.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "build/build_config.h"

namespace obermon {

ScramjetEngineService::ScramjetEngineService() = default;
ScramjetEngineService::~ScramjetEngineService() = default;

ScramjetEngineService* ScramjetEngineService::Get() {
  static base::NoDestructor<ScramjetEngineService> instance;
  return instance.get();
}

bool ScramjetEngineService::Start() {
  if (is_running()) {
    return true;
  }
  base::FilePath executable_dir;
  if (!base::PathService::Get(base::DIR_EXE, &executable_dir)) {
    return false;
  }
  const base::FilePath engine =
      executable_dir.Append(FILE_PATH_LITERAL("obermon"))
          .Append(FILE_PATH_LITERAL("scramjet-engine.exe"));
  if (!base::PathExists(engine)) {
    return false;
  }
  base::CommandLine command(engine);
  command.AppendSwitch("server");
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  process_ = base::LaunchProcess(command, options);
  return process_.IsValid();
}

void ScramjetEngineService::Shutdown() {
  if (process_.IsValid()) {
    process_.Terminate(0, false);
    process_.Close();
  }
}

bool ScramjetEngineService::is_running() const {
  return process_.IsValid() && process_.IsRunning();
}

}  // namespace obermon
