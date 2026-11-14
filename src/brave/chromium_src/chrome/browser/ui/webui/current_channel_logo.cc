/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#define CurrentChannelLogoResourceId CurrentChannelLogoResourceId_Unused
#include "base/notreached.h"

#include "components/grit/brave_components_resources.h"

#include <chrome/browser/ui/webui/current_channel_logo.cc>
#undef CurrentChannelLogoResourceId

namespace webui {

int CurrentChannelLogoResourceId() {
  // Use Anar's product mark anywhere Chromium requests the browser's channel
  // logo (including the Settings header and About page).
  return IDR_ANAR_NTP_LOGO_DARK_BG;
}

}  // namespace webui
