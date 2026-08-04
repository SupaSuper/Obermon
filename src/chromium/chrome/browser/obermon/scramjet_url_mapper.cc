// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_url_mapper.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/uuid.h"
#include "chrome/browser/obermon/constants.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

namespace obermon {
namespace {

base::flat_set<std::string>& AuthorizedSessions() {
  static base::NoDestructor<base::flat_set<std::string>> sessions;
  return *sessions;
}

}  // namespace

bool ScramjetURLMapper::IsEligibleDestination(const GURL& url) {
  return url.is_valid() &&
         (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme)) &&
         !IsInternalURL(url);
}

bool ScramjetURLMapper::IsInternalURL(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) &&
         url.host_piece() == kScramjetEngineHost &&
         url.EffectiveIntPort() == kScramjetEnginePort;
}

GURL ScramjetURLMapper::ToInternalURL(const GURL& destination) {
  if (!IsEligibleDestination(destination)) {
    return GURL();
  }
  const std::string token = base::Uuid::GenerateRandomV4().AsLowercaseString();
  AuthorizedSessions().insert(token);
  const std::string escaped_destination =
      net::EscapeQueryParamValue(destination.spec(), true);
  const std::string escaped_token = net::EscapeQueryParamValue(token, true);
  return GURL("http://127.0.0.1:4141/?goto=" + escaped_destination +
              "&obermon_token=" + escaped_token + "&obermon=1");
}

std::optional<GURL> ScramjetURLMapper::DestinationFromInternalURL(
    const GURL& internal) {
  if (!IsInternalURL(internal)) {
    return std::nullopt;
  }
  std::string token;
  if (!net::GetValueForKeyInQuery(internal, "obermon_token", &token) ||
      !AuthorizedSessions().contains(token)) {
    return std::nullopt;
  }
  std::string value;
  if (!net::GetValueForKeyInQuery(internal, "goto", &value)) {
    return std::nullopt;
  }
  GURL destination(value);
  if (!IsEligibleDestination(destination)) {
    return std::nullopt;
  }
  return destination;
}

}  // namespace obermon
