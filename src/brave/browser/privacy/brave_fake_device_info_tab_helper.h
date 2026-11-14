/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_DEVICE_INFO_TAB_HELPER_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_DEVICE_INFO_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Applies (or clears) the "Fake device info" privacy setting on a tab: a
// fake OS/platform user agent plus a matching fake screen size, kept
// mutually consistent by always coming from the same preset (see
// brave_fake_device_info_presets.h).
class BraveFakeDeviceInfoTabHelper final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BraveFakeDeviceInfoTabHelper> {
 public:
  explicit BraveFakeDeviceInfoTabHelper(content::WebContents* contents);
  BraveFakeDeviceInfoTabHelper(const BraveFakeDeviceInfoTabHelper&) = delete;
  BraveFakeDeviceInfoTabHelper& operator=(
      const BraveFakeDeviceInfoTabHelper&) = delete;
  ~BraveFakeDeviceInfoTabHelper() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  void ApplyFakeDeviceInfoPref();

  PrefChangeRegistrar pref_change_registrar_;

  // Defers the initial ApplyFakeDeviceInfoPref() call until after this
  // WebContents has finished initializing — see the comment at the
  // constructor's PostTask call.
  base::WeakPtrFactory<BraveFakeDeviceInfoTabHelper> weak_factory_{this};
};

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_DEVICE_INFO_TAB_HELPER_H_
