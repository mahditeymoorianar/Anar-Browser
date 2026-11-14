/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#define BRAVE_CHROMIUM_SRC_CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_

// Declares WebContentsImpl's overrides of the new virtual methods injected
// into the public content::WebContents interface by this same directory's
// content/public/browser/web_contents.h override (SetFakeGeopositionOverride
// et al.) — an out-of-line `WebContentsImpl::Method() {}` definition is only
// valid C++ if WebContentsImpl itself (re-)declares the method, inherited
// declarations from the base interface alone aren't enough. Anchored on the
// adjacent ClearDeviceEmulationSize() declaration purely because it's a
// convenient existing zero-arg void method to hang the macro-substitution
// trick on; unrelated to it otherwise.
#define ClearDeviceEmulationSize(...)                                     \
  SetFakeGeopositionOverride(double latitude, double longitude) override; \
  void ClearFakeGeopositionOverride() override;                          \
  void BraveSetDeviceEmulationSize(const gfx::Size& size) override;      \
  void BraveClearDeviceEmulationSize() override;                         \
  void ClearDeviceEmulationSize(__VA_ARGS__)

#include <content/browser/web_contents/web_contents_impl.h>  // IWYU pragma: export

#undef ClearDeviceEmulationSize

#endif  // BRAVE_CHROMIUM_SRC_CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
