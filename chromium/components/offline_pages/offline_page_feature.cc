// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_feature.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/offline_pages/offline_page_switches.h"
#include "components/version_info/version_info.h"

namespace offline_pages {

namespace {
const char kOfflinePagesFieldTrialName[] = "OfflinePages";
// The old experiment has only one mode to enable offline pages.
const char kEnabledGroupName[] = "Enabled";
// The new experiment supports two modes for offline pages.
const char kEnabledAsBookmarksGroupName[] = "EnabledAsBookmarks";
const char kEnabledAsSavedPagesGroupName[] = "EnabledAsSavedPages";
}  // namespace

const base::Feature kOffliningRecentPagesFeature {
   "offline-recent-pages", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kOfflinePagesBackgroundLoadingFeature {
   "offline-pages-background-loading", base::FEATURE_DISABLED_BY_DEFAULT
};

FeatureMode GetOfflinePageFeatureMode() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  std::string group_name =
      base::FieldTrialList::FindFullName(kOfflinePagesFieldTrialName);

  // The old experiment 'Enabled' defaults to showing saved page.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfflinePages)) {
    return FeatureMode::ENABLED_AS_SAVED_PAGES;
  }
  // The new experiment can control showing either bookmark or saved page.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfflinePagesAsBookmarks)) {
    return FeatureMode::ENABLED_AS_BOOKMARKS;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfflinePagesAsSavedPages)) {
    return FeatureMode::ENABLED_AS_SAVED_PAGES;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfflinePages)) {
    return FeatureMode::DISABLED;
  }

  // The old experiment 'Enabled' defaults to showing saved page.
  if (group_name == kEnabledGroupName)
    return FeatureMode::ENABLED_AS_SAVED_PAGES;
  // The new experiment can control showing either bookmark or saved page.
  if (base::StartsWith(group_name, kEnabledAsBookmarksGroupName,
                       base::CompareCase::SENSITIVE)) {
    return FeatureMode::ENABLED_AS_BOOKMARKS;
  }
  if (base::StartsWith(group_name, kEnabledAsSavedPagesGroupName,
                       base::CompareCase::SENSITIVE)) {
    return FeatureMode::ENABLED_AS_SAVED_PAGES;
  }

  // Enabled by default on trunk.
  return version_info::IsOfficialBuild() ? FeatureMode::DISABLED
                                         : FeatureMode::ENABLED_AS_BOOKMARKS;
}

bool IsOfflinePagesEnabled() {
  FeatureMode mode = GetOfflinePageFeatureMode();
  return mode == FeatureMode::ENABLED_AS_BOOKMARKS ||
         mode == FeatureMode::ENABLED_AS_SAVED_PAGES;
}

bool IsOffliningRecentPagesEnabled() {
  return  base::FeatureList::IsEnabled(kOffliningRecentPagesFeature) &&
          IsOfflinePagesEnabled();
}

bool IsOfflinePagesBackgroundLoadingEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesBackgroundLoadingFeature)
      && IsOfflinePagesEnabled();
}

}  // namespace offline_pages
