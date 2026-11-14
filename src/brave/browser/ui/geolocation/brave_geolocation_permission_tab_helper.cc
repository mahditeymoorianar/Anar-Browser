/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/geolocation/brave_geolocation_permission_tab_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "brave/components/constants/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

BraveGeolocationPermissionTabHelper::BraveGeolocationPermissionTabHelper(
    content::WebContents* contents)
    : WebContentsObserver(contents),
      content::WebContentsUserData<BraveGeolocationPermissionTabHelper>(
          *contents),
      brave_geolocation_permission_receivers_(contents, this) {
  if (auto* profile =
          Profile::FromBrowserContext(contents->GetBrowserContext())) {
    pref_change_registrar_.Init(profile->GetPrefs());
    auto callback = base::BindRepeating(
        &BraveGeolocationPermissionTabHelper::ApplyFakeGeolocationPref,
        base::Unretained(this));
    pref_change_registrar_.Add(kFakeGeolocationEnabled, callback);
    pref_change_registrar_.Add(kFakeGeolocationLatitude, callback);
    pref_change_registrar_.Add(kFakeGeolocationLongitude, callback);
  }
  // Don't call ApplyFakeGeolocationPref() synchronously here: this
  // constructor runs while |contents| itself is still being constructed
  // (tab helpers are attached mid-WebContents::Create()), and
  // ApplyFakeGeolocationPref() reaches into
  // WebContents::SetFakeGeopositionOverride()/GetGeolocationContext(),
  // which isn't safe to call until the WebContents has finished
  // initializing. Defer the initial application by one turn of the
  // message loop instead.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BraveGeolocationPermissionTabHelper::ApplyFakeGeolocationPref,
                      weak_factory_.GetWeakPtr()));
}

BraveGeolocationPermissionTabHelper::~BraveGeolocationPermissionTabHelper() =
    default;

void BraveGeolocationPermissionTabHelper::ApplyFakeGeolocationPref() {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return;
  }
  const PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(kFakeGeolocationEnabled)) {
    web_contents()->SetFakeGeopositionOverride(
        prefs->GetDouble(kFakeGeolocationLatitude),
        prefs->GetDouble(kFakeGeolocationLongitude));
  } else {
    web_contents()->ClearFakeGeopositionOverride();
  }
}

// static
void BraveGeolocationPermissionTabHelper::BindBraveGeolocationPermission(
    mojo::PendingAssociatedReceiver<
        geolocation::mojom::BraveGeolocationPermission> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }

  auto* tab_helper =
      BraveGeolocationPermissionTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    return;
  }
  tab_helper->brave_geolocation_permission_receivers_.Bind(rfh,
                                                           std::move(receiver));
}

void BraveGeolocationPermissionTabHelper::PrimaryPageChanged(
    content::Page& page) {
  enable_high_accuracy_ = false;
  ApplyFakeGeolocationPref();
}

void BraveGeolocationPermissionTabHelper::SetEnableHighAccuracy(
    bool enable_high_accuracy) {
  enable_high_accuracy_ = enable_high_accuracy;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BraveGeolocationPermissionTabHelper);
