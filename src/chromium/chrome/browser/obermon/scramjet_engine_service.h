// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_ENGINE_SERVICE_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_ENGINE_SERVICE_H_

#include "base/process/process.h"

namespace obermon {

class ScramjetEngineService {
 public:
  static ScramjetEngineService* Get();
  ScramjetEngineService(const ScramjetEngineService&) = delete;
  ScramjetEngineService& operator=(const ScramjetEngineService&) = delete;

  bool Start();
  void Shutdown();
  bool is_running() const;

 private:
  ScramjetEngineService();
  ~ScramjetEngineService();
  base::Process process_;
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_ENGINE_SERVICE_H_
