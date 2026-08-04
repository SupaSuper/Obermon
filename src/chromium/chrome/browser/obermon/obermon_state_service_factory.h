// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_OBERMON_STATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OBERMON_OBERMON_STATE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace obermon {

class ObermonStateService;

// Owns one canonical Obermon state graph for each regular or incognito
// profile. Separate OTR instances prevent private-window state and diagnostics
// from leaking into the original profile.
class ObermonStateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ObermonStateServiceFactory* GetInstance();
  static ObermonStateService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<ObermonStateServiceFactory>;

  ObermonStateServiceFactory();
  ~ObermonStateServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace obermon

#endif  // CHROME_BROWSER_OBERMON_OBERMON_STATE_SERVICE_FACTORY_H_
