/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_GEOLOCATION_BRAVE_GEOLOCATION_PERMISSION_TAB_HELPER_H_
#define BRAVE_BROWSER_UI_GEOLOCATION_BRAVE_GEOLOCATION_PERMISSION_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "brave/components/brave_geolocation_permission/common/brave_geolocation_permission.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class BraveGeolocationPermissionTabHelper final
    : public content::WebContentsObserver,
      public geolocation::mojom::BraveGeolocationPermission,
      public content::WebContentsUserData<BraveGeolocationPermissionTabHelper> {
 public:
  explicit BraveGeolocationPermissionTabHelper(content::WebContents* contents);
  ~BraveGeolocationPermissionTabHelper() override;

  static void BindBraveGeolocationPermission(
      mojo::PendingAssociatedReceiver<
          geolocation::mojom::BraveGeolocationPermission> receiver,
      content::RenderFrameHost* rfh);

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // geolocation::mojom::BraveGeolocationPermission overrides:
  void SetEnableHighAccuracy(bool high_accuracy) override;

  bool enable_high_accuracy() const { return enable_high_accuracy_; }

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  // Applies (or clears) the fake geolocation override on |web_contents()|
  // based on the current "Fake geolocation" privacy prefs.
  void ApplyFakeGeolocationPref();

  content::RenderFrameHostReceiverSet<
      geolocation::mojom::BraveGeolocationPermission>
      brave_geolocation_permission_receivers_;

  bool enable_high_accuracy_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  // Used to defer the initial ApplyFakeGeolocationPref() call until after
  // this WebContents (and its constructor, where this tab helper itself is
  // created) has finished initializing. Calling into
  // WebContents::SetFakeGeopositionOverride()/GetGeolocationContext()
  // synchronously from this helper's own constructor was reaching into
  // WebContentsImpl before its internal frame-tree state was ready,
  // causing an early-startup crash.
  base::WeakPtrFactory<BraveGeolocationPermissionTabHelper> weak_factory_{
      this};
};

#endif  // BRAVE_BROWSER_UI_GEOLOCATION_BRAVE_GEOLOCATION_PERMISSION_TAB_HELPER_H_
