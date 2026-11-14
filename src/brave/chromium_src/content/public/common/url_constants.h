/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
#define BRAVE_CHROMIUM_SRC_CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
// ios cannot include content deps
namespace content {
inline constexpr char kChromeUIScheme[] = "chrome";
}
#else
#include <content/public/common/url_constants.h>  // IWYU pragma: export
#endif

namespace content {
inline constexpr char kBraveUIScheme[] = "brave";
// Anar's primary internal-page scheme: what's shown in the address bar for
// internal pages (chrome:// remains the true internal/routing scheme;
// brave:// and anar:// are both display/input aliases normalized to it —
// see browser_about_handler.cc, omnibox_text_util.cc, etc.). Typed or saved
// brave:// URLs keep working; new navigations display as anar://.
inline constexpr char kAnarUIScheme[] = "anar";
}

#endif  // BRAVE_CHROMIUM_SRC_CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
