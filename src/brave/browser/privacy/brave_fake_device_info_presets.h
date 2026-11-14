/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_DEVICE_INFO_PRESETS_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_DEVICE_INFO_PRESETS_H_

#include <string>

#include "base/containers/span.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/geometry/size.h"

namespace brave_privacy {

// A bundle of the OS/platform-identifying signals a website can read, all
// changed together so they stay mutually consistent (a Windows user agent
// paired with a "MacIntel" platform string is itself a fingerprinting
// tell). Picking one of these presets from Settings is deliberately the
// only supported way to spoof device info — offering every field as an
// independently-editable text box would make it trivial for a user to
// produce an inconsistent, actually-more-identifiable combination.
struct FakeDeviceInfoPreset {
  // Value stored in prefs::kFakeDeviceInfoPreset.
  const char* id;
  // The <select> option shown to the user.
  const char* display_name;
  // Substituted for the OS token inside the real user agent string's first
  // parentheses, e.g. the "Windows NT 10.0; Win64; x64" in
  // "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 ...".
  const char* ua_os_token;
  // navigator.platform / Sec-CH-UA-Platform.
  const char* platform;
  const char* platform_version;
  const char* architecture;
  bool mobile;
  gfx::Size screen_size;
};

// Returns the built-in presets, in the order they should be offered in
// Settings. Never empty.
base::span<const FakeDeviceInfoPreset> GetFakeDeviceInfoPresets();

// Returns the preset matching |id|, or the first preset if |id| is unknown
// (e.g. a pref value from a build that has since removed that preset).
const FakeDeviceInfoPreset& GetFakeDeviceInfoPreset(const std::string& id);

// Builds the full user agent override (string + client hints) for |preset|,
// based on the browser's real user agent so the Chrome/WebKit version
// tokens always match this build rather than being hand-maintained.
blink::UserAgentOverride BuildUserAgentOverride(
    const FakeDeviceInfoPreset& preset);

}  // namespace brave_privacy

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_DEVICE_INFO_PRESETS_H_
