// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_features.h"

#include "base/feature_list.h"

namespace personal_context::features {

// kPersonalContext feature state is enforced via plaster rewrite.
BASE_FEATURE(kPersonalContext, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalContextLogNonEligibilityUma,
             base::FEATURE_DISABLED_BY_DEFAULT);


}  // namespace personal_context::features
