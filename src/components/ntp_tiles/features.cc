// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace ntp_tiles {

const char kPopularSitesFieldTrialName[] = "NTPPopularSites";

// kPopularSitesBakedInContentFeature feature state is enforced via plaster rewrite.
BASE_FEATURE(kPopularSitesBakedInContentFeature,
             "NTPPopularSitesBakedInContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// kNtpMostLikelyFaviconsFromServerFeature feature state is enforced via plaster rewrite.
BASE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature,
             "NTPMostLikelyFaviconsFromServer",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPopularSitesRefreshUs, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kPopularSitesRefreshUsArm{&kPopularSitesRefreshUs,
                                                        "arm", 0};

}  // namespace ntp_tiles
