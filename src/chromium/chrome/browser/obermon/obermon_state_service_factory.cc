// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/obermon_state_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/obermon/obermon_state_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace obermon {

ObermonStateServiceFactory::ObermonStateServiceFactory()
    : ProfileKeyedServiceFactory(
          "ObermonStateServiceFactory",
          ProfileSelections::BuildForRegularAndIncognito()) {}

ObermonStateServiceFactory::~ObermonStateServiceFactory() = default;

// static
ObermonStateServiceFactory* ObermonStateServiceFactory::GetInstance() {
  static base::NoDestructor<ObermonStateServiceFactory> instance;
  return instance.get();
}

// static
ObermonStateService* ObermonStateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ObermonStateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
ObermonStateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ObermonStateService>(
      Profile::FromBrowserContext(context));
}

}  // namespace obermon
