// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_OBERMON_BACKEND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OBERMON_OBERMON_BACKEND_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace obermon {

class ObermonBackendService;

class ObermonBackendServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ObermonBackendServiceFactory* GetInstance();
  static ObermonBackendService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<ObermonBackendServiceFactory>;

  ObermonBackendServiceFactory();
  ~ObermonBackendServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace obermon

#endif  // CHROME_BROWSER_OBERMON_OBERMON_BACKEND_SERVICE_FACTORY_H_
