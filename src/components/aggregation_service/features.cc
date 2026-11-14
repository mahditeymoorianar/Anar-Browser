// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace aggregation_service {

// kAggregationServiceMultipleCloudProviders feature state is enforced via plaster rewrite.
BASE_FEATURE(kAggregationServiceMultipleCloudProviders,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAggregationServiceCoordinatorAllowlist{
    &kAggregationServiceMultipleCloudProviders, "allowlist", ""};

}  // namespace aggregation_service
