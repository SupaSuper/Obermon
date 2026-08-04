// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/obermon_backend_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/obermon/obermon_backend_service.h"
#include "chrome/browser/obermon/obermon_state_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace obermon {

ObermonBackendServiceFactory::ObermonBackendServiceFactory()
    : ProfileKeyedServiceFactory(
          "ObermonBackendServiceFactory",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(ObermonStateServiceFactory::GetInstance());
}

ObermonBackendServiceFactory::~ObermonBackendServiceFactory() = default;

// static
ObermonBackendServiceFactory* ObermonBackendServiceFactory::GetInstance() {
  static base::NoDestructor<ObermonBackendServiceFactory> instance;
  return instance.get();
}

// static
ObermonBackendService* ObermonBackendServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ObermonBackendService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
ObermonBackendServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ObermonBackendService>(
      profile, ObermonStateServiceFactory::GetForProfile(profile));
}

}  // namespace obermon
