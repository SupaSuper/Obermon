// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#ifndef CHROME_BROWSER_OBERMON_SCRAMJET_URL_MAPPER_H_
#define CHROME_BROWSER_OBERMON_SCRAMJET_URL_MAPPER_H_

#include <optional>
#include "url/gurl.h"

namespace obermon {

class ScramjetURLMapper {
 public:
  static bool IsEligibleDestination(const GURL& url);
  static bool IsInternalURL(const GURL& url);
  static GURL ToInternalURL(const GURL& destination);
  static std::optional<GURL> DestinationFromInternalURL(const GURL& internal);
};

}  // namespace obermon
#endif  // CHROME_BROWSER_OBERMON_SCRAMJET_URL_MAPPER_H_
