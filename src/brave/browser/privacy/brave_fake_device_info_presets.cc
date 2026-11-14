/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_fake_device_info_presets.h"

#include "components/embedder_support/user_agent_utils.h"

namespace brave_privacy {

namespace {

constexpr FakeDeviceInfoPreset kPresets[] = {
    {
        "windows",
        "Windows desktop",
        "Windows NT 10.0; Win64; x64",
        "Windows",
        "10.0.0",
        "x86",
        /*mobile=*/false,
        gfx::Size(1920, 1080),
    },
    {
        "macos",
        "macOS desktop",
        "Macintosh; Intel Mac OS X 10_15_7",
        "macOS",
        "14.5.0",
        "arm",
        /*mobile=*/false,
        gfx::Size(1728, 1117),
    },
    {
        "linux",
        "Linux desktop",
        "X11; Linux x86_64",
        "Linux",
        "",
        "x86",
        /*mobile=*/false,
        gfx::Size(1920, 1080),
    },
    {
        "android",
        "Android phone",
        "Linux; Android 14; Pixel 8",
        "Android",
        "14.0.0",
        "",
        /*mobile=*/true,
        gfx::Size(412, 915),
    },
};

}  // namespace

base::span<const FakeDeviceInfoPreset> GetFakeDeviceInfoPresets() {
  return base::span(kPresets);
}

const FakeDeviceInfoPreset& GetFakeDeviceInfoPreset(const std::string& id) {
  for (const auto& preset : kPresets) {
    if (id == preset.id) {
      return preset;
    }
  }
  return kPresets[0];
}

blink::UserAgentOverride BuildUserAgentOverride(
    const FakeDeviceInfoPreset& preset) {
  blink::UserAgentOverride ua_override;

  // Start from the browser's real UA string so the Chrome/WebKit version
  // tokens are always correct for this build, then swap only the part
  // inside the first parentheses (the OS/platform token) for the preset's.
  const std::string real_ua = embedder_support::GetUserAgent();
  const size_t open = real_ua.find('(');
  const size_t close = real_ua.find(')', open == std::string::npos ? 0 : open);
  if (open != std::string::npos && close != std::string::npos &&
      close > open) {
    ua_override.ua_string_override =
        real_ua.substr(0, open + 1) + preset.ua_os_token +
        real_ua.substr(close);
  } else {
    // Real UA didn't look like a normal browser UA string (shouldn't
    // happen); fall back to it unchanged rather than producing garbage.
    ua_override.ua_string_override = real_ua;
  }

  // Keep the real brand/version client hints (they identify this browser,
  // not the OS) and only override the OS-identifying fields.
  blink::UserAgentMetadata metadata = embedder_support::GetUserAgentMetadata();
  metadata.platform = preset.platform;
  metadata.platform_version = preset.platform_version;
  metadata.architecture = preset.architecture;
  metadata.mobile = preset.mobile;
  metadata.model = "";
  metadata.form_factors = {preset.mobile ? blink::kMobileFormFactor
                                          : blink::kDesktopFormFactor};
  ua_override.ua_metadata_override = std::move(metadata);

  return ua_override;
}

}  // namespace brave_privacy
