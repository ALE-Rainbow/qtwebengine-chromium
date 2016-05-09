// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_util.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace data_util {

TEST(AutofillDataUtilTest, SplitName) {
  typedef struct {
    std::string full_name;
    std::string given_name;
    std::string middle_name;
    std::string family_name;

  } TestCase;

  TestCase test_cases[] = {
      // Full name including given, middle and family names.
      {"Homer Jay Simpson", "Homer", "Jay", "Simpson"},
      // No middle name.
      {"Moe Szyslak", "Moe", "", "Szyslak"},
      // Common name prefixes removed.
      {"Reverend Timothy Lovejoy", "Timothy", "", "Lovejoy"},
      // Common name suffixes removed.
      {"John Frink Phd", "John", "", "Frink"},
      // Exception to the name suffix removal.
      {"John Ma", "John", "", "Ma"},
      // Common family name prefixes not considered a middle name.
      {"Milhouse Van Houten", "Milhouse", "", "Van Houten"}};

  for (TestCase test_case : test_cases) {
    NameParts name_parts = SplitName(base::UTF8ToUTF16(test_case.full_name));

    EXPECT_EQ(base::UTF8ToUTF16(test_case.given_name), name_parts.given);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.middle_name), name_parts.middle);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.family_name), name_parts.family);
  }
}

}  // namespace data_util
}  // namespace autofill
