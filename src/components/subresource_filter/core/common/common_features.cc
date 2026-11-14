// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/common_features.h"

namespace subresource_filter {

// kAdTagging feature state is enforced via plaster rewrite.
BASE_FEATURE(kAdTagging, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSubresourceFilterPrewarm, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace subresource_filter
