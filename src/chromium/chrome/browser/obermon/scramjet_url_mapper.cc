// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_url_mapper.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/obermon/constants.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

namespace obermon {
namespace {

constexpr size_t kMaximumAuthorizedSessions = 4096;
constexpr base::TimeDelta kAuthorizedSessionLifetime = base::Hours(24);

class AuthorizedSessionRegistry {
 public:
  void Authorize(const std::string& token) {
    const base::TimeTicks now = base::TimeTicks::Now();
    Prune(now);
    sessions_[token] = now;
    order_.emplace_back(token, now);
    Prune(now);
  }

  bool ValidateAndTouch(const std::string& token) {
    const base::TimeTicks now = base::TimeTicks::Now();
    Prune(now);
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) {
      return false;
    }
    it->second = now;
    order_.emplace_back(token, now);
    return true;
  }

 private:
  void Prune(base::TimeTicks now) {
    while (!order_.empty()) {
      const auto& [token, recorded_at] = order_.front();
      const auto current = sessions_.find(token);
      const bool stale_queue_entry =
          current == sessions_.end() || current->second != recorded_at;
      const bool expired = now - recorded_at > kAuthorizedSessionLifetime;
      const bool over_limit = sessions_.size() > kMaximumAuthorizedSessions;
      if (!stale_queue_entry && !expired && !over_limit) {
        break;
      }
      if (current != sessions_.end() && current->second == recorded_at &&
          (expired || over_limit)) {
        sessions_.erase(current);
      }
      order_.pop_front();
    }
  }

  base::flat_map<std::string, base::TimeTicks> sessions_;
  base::circular_deque<std::pair<std::string, base::TimeTicks>> order_;
};

AuthorizedSessionRegistry& AuthorizedSessions() {
  static base::NoDestructor<AuthorizedSessionRegistry> sessions;
  return *sessions;
}

bool IsValidTransportPartition(std::string_view partition) {
  if (partition.empty() || partition.size() > 128) {
    return false;
  }
  for (const char character : partition) {
    const bool valid = character == '-' || character == '_' ||
                       (character >= '0' && character <= '9') ||
                       (character >= 'a' && character <= 'z') ||
                       (character >= 'A' && character <= 'Z');
    if (!valid) {
      return false;
    }
  }
  return true;
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

GURL ScramjetURLMapper::ToInternalURL(
    const GURL& destination,
    std::string_view transport_partition) {
  if (!IsEligibleDestination(destination) ||
      !IsValidTransportPartition(transport_partition)) {
    return GURL();
  }
  const std::string token = base::Uuid::GenerateRandomV4().AsLowercaseString();
  AuthorizedSessions().Authorize(token);
  const std::string escaped_destination =
      net::EscapeQueryParamValue(destination.spec(), true);
  const std::string escaped_token = net::EscapeQueryParamValue(token, true);
  const std::string escaped_partition = net::EscapeQueryParamValue(
      std::string(transport_partition), true);
  return GURL("http://127.0.0.1:4141/?goto=" + escaped_destination +
              "&obermon_token=" + escaped_token +
              "&obermon_partition=" + escaped_partition + "&obermon=1");
}

std::optional<GURL> ScramjetURLMapper::DestinationFromInternalURL(
    const GURL& internal) {
  if (!IsInternalURL(internal)) {
    return std::nullopt;
  }
  std::string token;
  if (!net::GetValueForKeyInQuery(internal, "obermon_token", &token) ||
      !AuthorizedSessions().ValidateAndTouch(token)) {
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
