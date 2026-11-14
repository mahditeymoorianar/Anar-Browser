/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_VOICE_SWAP_TAB_HELPER_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_VOICE_SWAP_TAB_HELPER_H_

#include "base/callback_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Reflects the "Voice Swap" privacy prefs into this tab's WebPreferences
// (voice_swap_enabled / voice_swap_persona_id / voice_swap_port /
// voice_swap_token — see the .mojom's comment on those fields), which is
// how the renderer-side script injector (BraveVoiceSwapRenderFrameObserver)
// learns whether and how to intercept getUserMedia() for this page.
class BraveVoiceSwapTabHelper final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BraveVoiceSwapTabHelper> {
 public:
  explicit BraveVoiceSwapTabHelper(content::WebContents* contents);
  BraveVoiceSwapTabHelper(const BraveVoiceSwapTabHelper&) = delete;
  BraveVoiceSwapTabHelper& operator=(const BraveVoiceSwapTabHelper&) = delete;
  ~BraveVoiceSwapTabHelper() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  void ApplyVoiceSwapPref();

  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription process_ready_subscription_;
};

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_VOICE_SWAP_TAB_HELPER_H_
