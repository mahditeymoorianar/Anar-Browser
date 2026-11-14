/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_voice_swap_tab_helper.h"

#include "base/functional/bind.h"
#include "brave/browser/privacy/brave_voice_swap_process_manager.h"
#include "brave/components/constants/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

BraveVoiceSwapTabHelper::BraveVoiceSwapTabHelper(content::WebContents* contents)
    : WebContentsObserver(contents),
      content::WebContentsUserData<BraveVoiceSwapTabHelper>(*contents) {
  if (auto* profile =
          Profile::FromBrowserContext(contents->GetBrowserContext())) {
    pref_change_registrar_.Init(profile->GetPrefs());
    auto callback = base::BindRepeating(
        &BraveVoiceSwapTabHelper::ApplyVoiceSwapPref, base::Unretained(this));
    pref_change_registrar_.Add(kVoiceSwapEnabled, callback);
    pref_change_registrar_.Add(kVoiceSwapPersonaId, callback);
  }
  process_ready_subscription_ =
      BraveVoiceSwapProcessManager::GetInstance()->AddReadyCallback(
          base::BindRepeating(&BraveVoiceSwapTabHelper::ApplyVoiceSwapPref,
                              base::Unretained(this)));
  ApplyVoiceSwapPref();
}

BraveVoiceSwapTabHelper::~BraveVoiceSwapTabHelper() = default;

void BraveVoiceSwapTabHelper::PrimaryPageChanged(content::Page& page) {
  ApplyVoiceSwapPref();
}

void BraveVoiceSwapTabHelper::ApplyVoiceSwapPref() {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return;
  }
  const PrefService* prefs = profile->GetPrefs();
  auto* process_manager = BraveVoiceSwapProcessManager::GetInstance();

  const bool wanted = prefs->GetBoolean(kVoiceSwapEnabled);
  if (wanted) {
    // Idempotent: does nothing if already running/starting. Readiness is
    // asynchronous (the server takes a moment to start); ApplyVoiceSwapPref
    // gets called again via process_ready_subscription_ once it is, so this
    // tab will pick up the real port/token shortly after without the user
    // needing to do anything.
    process_manager->EnsureRunning();
  }

  blink::web_pref::WebPreferences web_prefs =
      web_contents()->GetOrCreateWebPreferences();
  const bool enabled = wanted && process_manager->IsReady();
  const std::string persona_id = prefs->GetString(kVoiceSwapPersonaId);
  if (web_prefs.voice_swap_enabled == enabled &&
      web_prefs.voice_swap_persona_id == persona_id &&
      web_prefs.voice_swap_port == process_manager->port() &&
      web_prefs.voice_swap_token == process_manager->token()) {
    return;
  }
  web_prefs.voice_swap_enabled = enabled;
  web_prefs.voice_swap_persona_id = persona_id;
  web_prefs.voice_swap_port = process_manager->port();
  web_prefs.voice_swap_token = process_manager->token();
  web_contents()->SetWebPreferences(web_prefs);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BraveVoiceSwapTabHelper);
