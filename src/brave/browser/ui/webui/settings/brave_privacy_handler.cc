/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/settings/brave_privacy_handler.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "base/values.h"
#include "brave/components/ai_chat/core/common/buildflags/buildflags.h"
#include "brave/components/brave_origin/buildflags/buildflags.h"
#include "brave/components/brave_shields/core/common/features.h"
#include "brave/components/constants/pref_names.h"
#include "brave/components/de_amp/common/features.h"
#include "brave/components/debounce/core/common/features.h"
#include "brave/components/google_sign_in_permission/google_sign_in_permission_util.h"
#include "brave/components/p3a/pref_names.h"
#include "brave/components/psst/buildflags/buildflags.h"
#include "brave/components/request_otr/common/buildflags/buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_buildflags.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

#if BUILDFLAG(ENABLE_AI_CHAT)
#include "brave/components/ai_chat/core/browser/utils.h"
#include "brave/components/ai_chat/core/common/features.h"
#endif

#if BUILDFLAG(ENABLE_REQUEST_OTR)
#include "brave/components/request_otr/common/features.h"
#endif

#if !BUILDFLAG(USE_GCM_FROM_PLATFORM)
#include "brave/browser/gcm_driver/brave_gcm_channel_status.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "brave/components/windows_recall/windows_recall.h"
#endif

#if BUILDFLAG(ENABLE_PSST)
#include "brave/components/psst/core/common/features.h"
#endif

BravePrivacyHandler::BravePrivacyHandler() {
  local_state_change_registrar_.Init(g_browser_process->local_state());
  local_state_change_registrar_.Add(
      kStatsReportingEnabled,
      base::BindRepeating(&BravePrivacyHandler::OnStatsUsagePingEnabledChanged,
                          base::Unretained(this)));
  local_state_change_registrar_.Add(
      p3a::kP3AEnabled,
      base::BindRepeating(&BravePrivacyHandler::OnP3AEnabledChanged,
                          base::Unretained(this)));
#if BUILDFLAG(IS_WIN)
  if (windows_recall::IsWindowsRecallAvailable()) {
    local_state_change_registrar_.Add(
        windows_recall::prefs::kWindowsRecallDisabled,
        base::BindRepeating(
            &BravePrivacyHandler::OnWindowsRecallDisabledChanged,
            base::Unretained(this)));
  }
#endif
}

BravePrivacyHandler::~BravePrivacyHandler() {
  local_state_change_registrar_.RemoveAll();
}

void BravePrivacyHandler::RegisterMessages() {
  profile_ = Profile::FromWebUI(web_ui());

  web_ui()->RegisterMessageCallback(
      "setP3AEnabled", base::BindRepeating(&BravePrivacyHandler::SetP3AEnabled,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getP3AEnabled", base::BindRepeating(&BravePrivacyHandler::GetP3AEnabled,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setStatsUsagePingEnabled",
      base::BindRepeating(&BravePrivacyHandler::SetStatsUsagePingEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getStatsUsagePingEnabled",
      base::BindRepeating(&BravePrivacyHandler::GetStatsUsagePingEnabled,
                          base::Unretained(this)));
#if BUILDFLAG(IS_WIN)
  if (windows_recall::IsWindowsRecallAvailable()) {
    web_ui()->RegisterMessageCallback(
        "isWindowsRecallDisabled",
        base::BindRepeating(&BravePrivacyHandler::GetLocalStateBooleanEnabled,
                            base::Unretained(this),
                            windows_recall::prefs::kWindowsRecallDisabled));
    web_ui()->RegisterMessageCallback(
        "setWindowsRecallDisabled",
        base::BindRepeating(&BravePrivacyHandler::SetLocalStateBooleanEnabled,
                            base::Unretained(this),
                            windows_recall::prefs::kWindowsRecallDisabled));
  }
#endif

  web_ui()->RegisterMessageCallback(
      "getVoiceSwapPersonas",
      base::BindRepeating(&BravePrivacyHandler::GetVoiceSwapPersonas,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addVoiceSwapPersona",
      base::BindRepeating(&BravePrivacyHandler::AddVoiceSwapPersona,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeVoiceSwapPersona",
      base::BindRepeating(&BravePrivacyHandler::RemoveVoiceSwapPersona,
                          base::Unretained(this)));
}

// static
void BravePrivacyHandler::AddLoadTimeData(content::WebUIDataSource* data_source,
                                          Profile* profile) {
#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
  data_source->AddBoolean("pushMessagingEnabledAtStartup", true);
#else
  gcm::BraveGCMChannelStatus* gcm_channel_status =
      gcm::BraveGCMChannelStatus::GetForProfile(profile);

  DCHECK(gcm_channel_status);
  data_source->AddBoolean("pushMessagingEnabledAtStartup",
                          gcm_channel_status->IsGCMEnabled());
#endif
  data_source->AddBoolean(
      "isDeAmpFeatureEnabled",
      base::FeatureList::IsEnabled(de_amp::features::kBraveDeAMP));
  data_source->AddBoolean(
      "isDebounceFeatureEnabled",
      base::FeatureList::IsEnabled(debounce::features::kBraveDebounce));
#if BUILDFLAG(ENABLE_REQUEST_OTR)
  data_source->AddBoolean(
      "isRequestOTRFeatureEnabled",
      base::FeatureList::IsEnabled(request_otr::features::kBraveRequestOTRTab));
#endif
  data_source->AddBoolean(
      "isPsstFeatureEnabled",
#if BUILDFLAG(ENABLE_PSST)
      base::FeatureList::IsEnabled(psst::features::kEnablePsst));
#else
      false);
#endif
  data_source->AddBoolean(
      "isGoogleSignInFeatureEnabled",
      google_sign_in_permission::IsGoogleSignInFeatureEnabled());
#if BUILDFLAG(ENABLE_AI_CHAT)
  data_source->AddBoolean(
      "isOpenAIChatFromBraveSearchEnabled",
      ai_chat::IsAIChatEnabled(profile->GetPrefs()) &&
          ai_chat::features::IsOpenAIChatFromBraveSearchEnabled());
#endif
#if BUILDFLAG(IS_BRAVE_ORIGIN_BRANDED)
  // P3A and usage ping (stats reporting) are disabled for Brave Origin branded
  // builds. Hide their settings unconditionally so they don't briefly appear on
  // first launch before the disabling policy is applied (brave-browser#56166).
  data_source->AddBoolean("isStatsReportingEnabledManaged", true);
  data_source->AddBoolean("isP3AEnabledManaged", true);
#else
  auto* local_state = g_browser_process->local_state();
  data_source->AddBoolean(
      "isStatsReportingEnabledManaged",
      local_state->IsManagedPreference(kStatsReportingEnabled));
  data_source->AddBoolean("isP3AEnabledManaged",
                          local_state->IsManagedPreference(p3a::kP3AEnabled));
#endif

#if BUILDFLAG(IS_WIN)
  {
    data_source->AddBoolean("isWindowsRecallAvailable",
                            windows_recall::IsWindowsRecallAvailable());
    data_source->AddBoolean("windowsRecallDisabledAtStartup",
                            windows_recall::IsWindowsRecallDisabled(
                                g_browser_process->local_state()));
  }
#else
  data_source->AddBoolean("isWindowsRecallAvailable", false);
#endif
}

void BravePrivacyHandler::SetLocalStateBooleanEnabled(
    const std::string& path,
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  if (!args[0].is_bool()) {
    return;
  }

  bool enabled = args[0].GetBool();
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(path, enabled);
}

void BravePrivacyHandler::GetLocalStateBooleanEnabled(
    const std::string& path,
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);

  PrefService* local_state = g_browser_process->local_state();
  bool enabled = local_state->GetBoolean(path);

  AllowJavascript();
  ResolveJavascriptCallback(args[0], base::Value(enabled));
}

void BravePrivacyHandler::SetStatsUsagePingEnabled(
    const base::ListValue& args) {
  SetLocalStateBooleanEnabled(kStatsReportingEnabled, args);
}

void BravePrivacyHandler::GetStatsUsagePingEnabled(
    const base::ListValue& args) {
  GetLocalStateBooleanEnabled(kStatsReportingEnabled, args);
}

void BravePrivacyHandler::OnStatsUsagePingEnabledChanged() {
  if (IsJavascriptAllowed()) {
    PrefService* local_state = g_browser_process->local_state();
    bool user_enabled = local_state->GetBoolean(kStatsReportingEnabled);
    bool is_managed = local_state->IsManagedPreference(kStatsReportingEnabled);

    FireWebUIListener("stats-usage-ping-enabled-changed", user_enabled,
                      is_managed);
  }
}

void BravePrivacyHandler::SetP3AEnabled(const base::ListValue& args) {
  SetLocalStateBooleanEnabled(p3a::kP3AEnabled, args);
}

void BravePrivacyHandler::GetP3AEnabled(const base::ListValue& args) {
  GetLocalStateBooleanEnabled(p3a::kP3AEnabled, args);
}

void BravePrivacyHandler::OnP3AEnabledChanged() {
  if (IsJavascriptAllowed()) {
    PrefService* local_state = g_browser_process->local_state();
    bool user_enabled = local_state->GetBoolean(p3a::kP3AEnabled);
    bool is_managed = local_state->IsManagedPreference(p3a::kP3AEnabled);

    FireWebUIListener("p3a-enabled-changed", user_enabled, is_managed);
  }
}

#if BUILDFLAG(IS_WIN)
void BravePrivacyHandler::OnWindowsRecallDisabledChanged() {
  CHECK(windows_recall::IsWindowsRecallAvailable());
  if (!IsJavascriptAllowed()) {
    return;
  }
  FireWebUIListener("windows-recall-disabled-changed",
                    base::Value(g_browser_process->local_state()->GetBoolean(
                        windows_recall::prefs::kWindowsRecallDisabled)));
}
#endif

namespace {

// See brave_voice_swap_process_manager.cc's identical helper — kept as a
// separate copy rather than a shared one since this handler and that
// process manager are otherwise unrelated and this is only a few lines.
base::FilePath GetVoiceSwapBackendDir() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::optional<std::string> override_dir =
      env->GetVar("ANAR_VOICE_SWAP_BACKEND_DIR");
  if (override_dir.has_value() && !override_dir->empty()) {
    return base::FilePath::FromUTF8Unsafe(*override_dir);
  }
  return base::FilePath(FILE_PATH_LITERAL(
      "C:\\projects\\voicer\\rvc-test\\backend"));
}

base::FilePath GetVoiceSwapRegistryPath() {
  return GetVoiceSwapBackendDir()
      .Append(FILE_PATH_LITERAL("app"))
      .Append(FILE_PATH_LITERAL("services"))
      .Append(FILE_PATH_LITERAL("models"))
      .Append(FILE_PATH_LITERAL("browser_custom"))
      .Append(FILE_PATH_LITERAL("registry.json"));
}

// Runs on a background thread (real file I/O, and the .pth being written
// can be well over 100MB — not something to do synchronously on the UI
// thread). Returns the merged persona list on success.
base::ListValue ReadVoiceSwapPersonasBlocking() {
  base::ListValue out;

  base::FilePath personas_json_path = GetVoiceSwapBackendDir()
                                          .Append(FILE_PATH_LITERAL("app"))
                                          .Append(FILE_PATH_LITERAL("data"))
                                          .Append(FILE_PATH_LITERAL("personas.json"));
  std::string builtin_contents;
  if (base::ReadFileToString(personas_json_path, &builtin_contents)) {
    if (auto parsed = base::JSONReader::Read(builtin_contents, base::JSON_PARSE_RFC);
        parsed && parsed->is_list()) {
      for (const auto& entry : parsed->GetList()) {
        if (!entry.is_dict()) {
          continue;
        }
        const auto& dict = entry.GetDict();
        base::DictValue row;
        row.Set("id", dict.FindString("id") ? *dict.FindString("id") : "");
        row.Set("name", dict.FindString("name") ? *dict.FindString("name") : "");
        row.Set("builtin", true);
        row.Set("hasIndex", dict.FindString("index_path") != nullptr &&
                                !dict.FindString("index_path")->empty());
        out.Append(std::move(row));
      }
    }
  }

  std::string custom_contents;
  if (base::ReadFileToString(GetVoiceSwapRegistryPath(), &custom_contents)) {
    if (auto parsed = base::JSONReader::Read(custom_contents, base::JSON_PARSE_RFC);
        parsed && parsed->is_list()) {
      for (const auto& entry : parsed->GetList()) {
        if (!entry.is_dict()) {
          continue;
        }
        const auto& dict = entry.GetDict();
        base::DictValue row;
        row.Set("id", dict.FindString("id") ? *dict.FindString("id") : "");
        row.Set("name", dict.FindString("name") ? *dict.FindString("name") : "");
        row.Set("builtin", false);
        const std::string* index_path = dict.FindString("index_path");
        row.Set("hasIndex", index_path != nullptr && !index_path->empty());
        out.Append(std::move(row));
      }
    }
  }

  return out;
}

// Returns the updated persona list on success, or an empty optional on
// failure (e.g. couldn't decode base64, couldn't write to disk).
std::optional<base::ListValue> AddVoiceSwapPersonaBlocking(
    std::string name,
    std::string pth_base64,
    std::string index_base64) {
  std::string pth_bytes;
  if (!base::Base64Decode(pth_base64, &pth_bytes) || pth_bytes.empty()) {
    return std::nullopt;
  }
  std::string index_bytes;
  bool has_index = !index_base64.empty();
  if (has_index && !base::Base64Decode(index_base64, &index_bytes)) {
    return std::nullopt;
  }

  base::FilePath models_dir = GetVoiceSwapRegistryPath().DirName();
  std::string persona_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::FilePath persona_dir = models_dir.AppendASCII(persona_id);
  if (!base::CreateDirectory(persona_dir)) {
    return std::nullopt;
  }

  base::FilePath pth_path = persona_dir.Append(FILE_PATH_LITERAL("model.pth"));
  if (!base::WriteFile(pth_path, pth_bytes)) {
    return std::nullopt;
  }
  std::string index_path_str;
  if (has_index) {
    base::FilePath index_path =
        persona_dir.Append(FILE_PATH_LITERAL("model.index"));
    if (base::WriteFile(index_path, index_bytes)) {
      index_path_str = index_path.AsUTF8Unsafe();
    }
  }

  base::ListValue registry;
  std::string existing_contents;
  if (base::ReadFileToString(GetVoiceSwapRegistryPath(), &existing_contents)) {
    if (auto parsed = base::JSONReader::Read(existing_contents, base::JSON_PARSE_RFC);
        parsed && parsed->is_list()) {
      registry = std::move(parsed->GetList());
    }
  }
  base::DictValue new_row;
  new_row.Set("id", persona_id);
  new_row.Set("name", name);
  new_row.Set("checkpoint_path", pth_path.AsUTF8Unsafe());
  new_row.Set("index_path", index_path_str);
  registry.Append(std::move(new_row));

  std::string registry_json;
  if (!base::JSONWriter::Write(registry, &registry_json) ||
      !base::CreateDirectory(models_dir) ||
      !base::WriteFile(GetVoiceSwapRegistryPath(), registry_json)) {
    return std::nullopt;
  }

  return ReadVoiceSwapPersonasBlocking();
}

std::optional<base::ListValue> RemoveVoiceSwapPersonaBlocking(
    std::string persona_id) {
  std::string existing_contents;
  if (!base::ReadFileToString(GetVoiceSwapRegistryPath(), &existing_contents)) {
    return std::nullopt;
  }
  auto parsed = base::JSONReader::Read(existing_contents, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_list()) {
    return std::nullopt;
  }
  base::ListValue updated;
  for (auto& entry : parsed->GetList()) {
    if (entry.is_dict()) {
      const std::string* id = entry.GetDict().FindString("id");
      if (id && *id == persona_id) {
        continue;  // dropped — this is the one being removed
      }
    }
    updated.Append(std::move(entry));
  }

  std::string registry_json;
  if (!base::JSONWriter::Write(updated, &registry_json) ||
      !base::WriteFile(GetVoiceSwapRegistryPath(), registry_json)) {
    return std::nullopt;
  }
  base::DeletePathRecursively(
      GetVoiceSwapRegistryPath().DirName().AppendASCII(persona_id));

  return ReadVoiceSwapPersonasBlocking();
}

}  // namespace

void BravePrivacyHandler::GetVoiceSwapPersonas(const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  base::Value callback_id = args[0].Clone();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadVoiceSwapPersonasBlocking),
      base::BindOnce(
          [](base::WeakPtr<BravePrivacyHandler> self, base::Value callback_id,
             base::ListValue personas) {
            if (self && self->IsJavascriptAllowed()) {
              self->ResolveJavascriptCallback(callback_id,
                                              base::Value(std::move(personas)));
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void BravePrivacyHandler::AddVoiceSwapPersona(const base::ListValue& args) {
  CHECK_EQ(args.size(), 4U);
  AllowJavascript();
  base::Value callback_id = args[0].Clone();
  const std::string name = args[1].GetString();
  const std::string pth_base64 = args[2].GetString();
  const std::string index_base64 = args[3].GetString();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AddVoiceSwapPersonaBlocking, name, pth_base64,
                     index_base64),
      base::BindOnce(
          [](base::WeakPtr<BravePrivacyHandler> self, base::Value callback_id,
             std::optional<base::ListValue> personas) {
            if (!self || !self->IsJavascriptAllowed()) {
              return;
            }
            if (personas) {
              self->ResolveJavascriptCallback(
                  callback_id, base::Value(std::move(*personas)));
            } else {
              self->RejectJavascriptCallback(callback_id, base::Value());
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void BravePrivacyHandler::RemoveVoiceSwapPersona(const base::ListValue& args) {
  CHECK_EQ(args.size(), 2U);
  AllowJavascript();
  base::Value callback_id = args[0].Clone();
  const std::string persona_id = args[1].GetString();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&RemoveVoiceSwapPersonaBlocking, persona_id),
      base::BindOnce(
          [](base::WeakPtr<BravePrivacyHandler> self, base::Value callback_id,
             std::optional<base::ListValue> personas) {
            if (!self || !self->IsJavascriptAllowed()) {
              return;
            }
            if (personas) {
              self->ResolveJavascriptCallback(
                  callback_id, base::Value(std::move(*personas)));
            } else {
              self->RejectJavascriptCallback(callback_id, base::Value());
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}
