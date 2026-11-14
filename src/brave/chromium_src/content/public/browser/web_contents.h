/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define BRAVE_CHROMIUM_SRC_CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_

#define IsLoading(...)                                                     \
  GetShouldDoLearningForTesting();                                         \
  virtual void SetFakeGeopositionOverride(double latitude, double longitude) {} \
  virtual void ClearFakeGeopositionOverride() {}                           \
  virtual void BraveSetDeviceEmulationSize(const gfx::Size& size) {}       \
  virtual void BraveClearDeviceEmulationSize() {}                          \
  virtual bool IsLoading(__VA_ARGS__)

#include <content/public/browser/web_contents.h>  // IWYU pragma: export

#undef IsLoading

#endif  // BRAVE_CHROMIUM_SRC_CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
