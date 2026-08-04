// Copyright 2026 Obermon contributors
// SPDX-License-Identifier: BSD-3-Clause
#include "chrome/browser/obermon/scramjet_url_mapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace obermon {

TEST(ScramjetURLMapperTest, RoundTripsAuthorizedDestination) {
  const GURL destination("https://example.com/a path?q=one%20two#fragment");
  const GURL internal = ScramjetURLMapper::ToInternalURL(destination);
  ASSERT_TRUE(internal.is_valid());
  const auto recovered =
      ScramjetURLMapper::DestinationFromInternalURL(internal);
  ASSERT_TRUE(recovered.has_value());
  EXPECT_EQ(destination, *recovered);
}

TEST(ScramjetURLMapperTest, RejectsNonWebSchemes) {
  EXPECT_FALSE(ScramjetURLMapper::IsEligibleDestination(GURL("file:///x")));
  EXPECT_FALSE(ScramjetURLMapper::IsEligibleDestination(
      GURL("chrome://settings/")));
}

TEST(ScramjetURLMapperTest, RejectsForgedInternalDestinationWithoutToken) {
  EXPECT_FALSE(ScramjetURLMapper::DestinationFromInternalURL(
                   GURL("http://127.0.0.1:4141/?goto=https%3A%2F%2Fexample.org"))
                   .has_value());
}

TEST(ScramjetURLMapperTest, RejectsWrongOriginEvenWithQuery) {
  EXPECT_FALSE(ScramjetURLMapper::DestinationFromInternalURL(
                   GURL("http://example.com/?goto=https%3A%2F%2Fexample.org&"
                        "obermon_token=forged"))
                   .has_value());
}

}  // namespace obermon
