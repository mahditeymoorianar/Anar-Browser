/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_WEBUI_SETTINGS_BRAVE_PRIVACY_HANDLER_H_
#define BRAVE_BROWSER_UI_WEBUI_SETTINGS_BRAVE_PRIVACY_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class WebUIDataSource;
}

class Profile;

class BravePrivacyHandler : public settings::SettingsPageUIHandler {
 public:
  BravePrivacyHandler();
  BravePrivacyHandler(const BravePrivacyHandler&) = delete;
  BravePrivacyHandler& operator=(const BravePrivacyHandler&) = delete;
  ~BravePrivacyHandler() override;
  static void AddLoadTimeData(content::WebUIDataSource* data_source,
                              Profile* profile);

 private:
  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

  void SetLocalStateBooleanEnabled(const std::string& path,
                                   const base::ListValue& args);
  void GetLocalStateBooleanEnabled(const std::string& path,
                                   const base::ListValue& args);

  void SetStatsUsagePingEnabled(const base::ListValue& args);
  void GetStatsUsagePingEnabled(const base::ListValue& args);
  void OnStatsUsagePingEnabledChanged();

  void SetP3AEnabled(const base::ListValue& args);
  void GetP3AEnabled(const base::ListValue& args);
  void OnP3AEnabledChanged();

#if BUILDFLAG(IS_WIN)
  void OnWindowsRecallDisabledChanged();
#endif

  // "Voice Swap" persona management. Reads/writes the same on-disk
  // registry the local voice-swap Python server (rvc-test/backend) reads,
  // rather than talking to that server over HTTP — avoids needing to
  // relax the settings page's CSP for a localhost fetch, and works even
  // when the server isn't currently running (e.g. the feature has never
  // been turned on yet this session). See
  // brave/browser/privacy/brave_voice_swap_process_manager.h's top
  // comment for why the RVC engine lives outside src/brave at all.
  void GetVoiceSwapPersonas(const base::ListValue& args);
  void AddVoiceSwapPersona(const base::ListValue& args);
  void RemoveVoiceSwapPersona(const base::ListValue& args);

  raw_ptr<Profile> profile_ = nullptr;
  PrefChangeRegistrar local_state_change_registrar_;

  base::WeakPtrFactory<BravePrivacyHandler> weak_ptr_factory_{this};
};

#endif  // BRAVE_BROWSER_UI_WEBUI_SETTINGS_BRAVE_PRIVACY_HANDLER_H_
