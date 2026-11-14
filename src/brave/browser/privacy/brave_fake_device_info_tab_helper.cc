/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_fake_device_info_tab_helper.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "brave/browser/privacy/brave_fake_device_info_presets.h"
#include "brave/components/constants/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

BraveFakeDeviceInfoTabHelper::BraveFakeDeviceInfoTabHelper(
    content::WebContents* contents)
    : WebContentsObserver(contents),
      content::WebContentsUserData<BraveFakeDeviceInfoTabHelper>(*contents) {
  if (auto* profile =
          Profile::FromBrowserContext(contents->GetBrowserContext())) {
    pref_change_registrar_.Init(profile->GetPrefs());
    auto callback = base::BindRepeating(
        &BraveFakeDeviceInfoTabHelper::ApplyFakeDeviceInfoPref,
        base::Unretained(this));
    pref_change_registrar_.Add(kFakeDeviceInfoEnabled, callback);
    pref_change_registrar_.Add(kFakeDeviceInfoPreset, callback);
  }
  // Don't call ApplyFakeDeviceInfoPref() synchronously here: this
  // constructor runs while |contents| is still being constructed (tab
  // helpers are attached mid-WebContents::Create()), and
  // ApplyFakeDeviceInfoPref() reaches into
  // WebContents::BraveSetDeviceEmulationSize()/BraveClearDeviceEmulationSize(),
  // which isn't safe to call until the WebContents has finished
  // initializing. Defer the initial application by one turn of the
  // message loop instead.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BraveFakeDeviceInfoTabHelper::ApplyFakeDeviceInfoPref,
                      weak_factory_.GetWeakPtr()));
}

BraveFakeDeviceInfoTabHelper::~BraveFakeDeviceInfoTabHelper() = default;

void BraveFakeDeviceInfoTabHelper::PrimaryPageChanged(content::Page& page) {
  ApplyFakeDeviceInfoPref();
}

void BraveFakeDeviceInfoTabHelper::ApplyFakeDeviceInfoPref() {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return;
  }
  const PrefService* prefs = profile->GetPrefs();

  if (!prefs->GetBoolean(kFakeDeviceInfoEnabled)) {
    web_contents()->SetUserAgentOverride(blink::UserAgentOverride(),
                                          /*override_in_new_tabs=*/false);
    web_contents()->BraveClearDeviceEmulationSize();
    return;
  }

  const auto& preset = brave_privacy::GetFakeDeviceInfoPreset(
      prefs->GetString(kFakeDeviceInfoPreset));
  web_contents()->SetUserAgentOverride(
      brave_privacy::BuildUserAgentOverride(preset),
      /*override_in_new_tabs=*/true);
  web_contents()->BraveSetDeviceEmulationSize(preset.screen_size);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BraveFakeDeviceInfoTabHelper);
