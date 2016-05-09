// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";

}  // namespace

namespace safe_browsing {

class SafeBrowsingV4ProtocolManagerUtilTest : public testing::Test {
 protected:
  void PopulateV4ProtocolConfig(V4ProtocolConfig* config) {
    config->client_name = kClient;
    config->version = kAppVer;
    config->key_param = kKeyParam;
  }
};

TEST_F(SafeBrowsingV4ProtocolManagerUtilTest, TestBackOffLogic) {
  size_t error_count = 0, back_off_multiplier = 1;

  // 1 error.
  base::TimeDelta next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(1U, error_count);
  EXPECT_EQ(1U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(15), next);
  EXPECT_GE(TimeDelta::FromMinutes(30), next);

  // 2 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(2U, error_count);
  EXPECT_EQ(2U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(30), next);
  EXPECT_GE(TimeDelta::FromMinutes(60), next);

  // 3 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(3U, error_count);
  EXPECT_EQ(4U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(60), next);
  EXPECT_GE(TimeDelta::FromMinutes(120), next);

  // 4 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(4U, error_count);
  EXPECT_EQ(8U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(120), next);
  EXPECT_GE(TimeDelta::FromMinutes(240), next);

  // 5 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(5U, error_count);
  EXPECT_EQ(16U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(240), next);
  EXPECT_GE(TimeDelta::FromMinutes(480), next);

  // 6 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(6U, error_count);
  EXPECT_EQ(32U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(480), next);
  EXPECT_GE(TimeDelta::FromMinutes(960), next);

  // 7 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(7U, error_count);
  EXPECT_EQ(64U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(960), next);
  EXPECT_GE(TimeDelta::FromMinutes(1920), next);

  // 8 errors, reached max backoff.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(8U, error_count);
  EXPECT_EQ(128U, back_off_multiplier);
  EXPECT_EQ(TimeDelta::FromHours(24), next);

  // 9 errors, reached max backoff and multiplier capped.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(9U, error_count);
  EXPECT_EQ(128U, back_off_multiplier);
  EXPECT_EQ(TimeDelta::FromHours(24), next);
}

TEST_F(SafeBrowsingV4ProtocolManagerUtilTest, TestGetRequestUrl) {
  V4ProtocolConfig config;
  PopulateV4ProtocolConfig(&config);

  std::string expectedUrl =
      "https://safebrowsing.googleapis.com/v4/someMethod/request_base64?"
      "alt=proto&client_id=unittest&client_version=1.0&key=test_key_param";
  EXPECT_EQ(expectedUrl, V4ProtocolManagerUtil::GetRequestUrl(
                             "request_base64", "someMethod", config)
                             .spec());
}

}  // namespace safe_browsing
