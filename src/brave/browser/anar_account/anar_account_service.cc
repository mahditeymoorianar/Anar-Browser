/* Copyright (c) 2026 The Anar Browser Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/anar_account/anar_account_service.h"

#include <utility>
#include <algorithm>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace anar_account {

namespace {

inline constexpr char kAccountsPref[] = "anar.account.encrypted_accounts";
inline constexpr char kPrimaryUserIdPref[] = "anar.account.primary_user_id";

}  // namespace

AnarAccountService::AnarAccountService(
    PrefService* prefs,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : prefs_(prefs) {
  if (os_crypt_async) {
    os_crypt_async->GetInstance(base::BindOnce(
        &AnarAccountService::OnEncryptorReady, weak_factory_.GetWeakPtr()));
  }
}

AnarAccountService::~AnarAccountService() = default;

// static
void AnarAccountService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kAccountsPref, std::string());
  registry->RegisterStringPref(kPrimaryUserIdPref, std::string());
}

void AnarAccountService::OnEncryptorReady(
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  encryptor_ = std::move(encryptor);
  if (!encryptor_ || !prefs_) {
    return;
  }
  const std::string encoded = prefs_->GetString(kAccountsPref);
  std::string ciphertext;
  std::string plaintext;
  if (!encoded.empty() && base::Base64Decode(encoded, &ciphertext) &&
      encryptor_->DecryptString(ciphertext, &plaintext)) {
    auto list = base::JSONReader::ReadList(plaintext, base::JSON_PARSE_RFC);
    if (list) {
      for (const auto& item : *list) {
        const auto* dict = item.GetIfDict();
        if (!dict) continue;
        StoredAccount account;
        if (const auto* value = dict->FindString("userId"))
          account.info.user_id = *value;
        if (const auto* value = dict->FindString("username"))
          account.info.username = *value;
        if (const auto* value = dict->FindString("nickname"))
          account.info.nickname = *value;
        if (const auto* value = dict->FindString("avatarUrl"))
          account.info.avatar_url = *value;
        if (const auto* value = dict->FindString("keys"))
          account.keys_json = *value;
        if (!account.info.user_id.empty()) accounts_.push_back(std::move(account));
      }
    }
  }
  if (pending_account_) {
    StoredAccount pending = std::move(*pending_account_);
    pending_account_.reset();
    auto it = std::ranges::find_if(accounts_, [&pending](const auto& account) {
      return account.info.user_id == pending.info.user_id;
    });
    if (it == accounts_.end()) accounts_.push_back(std::move(pending));
    else *it = std::move(pending);
    if (prefs_->GetString(kPrimaryUserIdPref).empty())
      prefs_->SetString(kPrimaryUserIdPref, accounts_.back().info.user_id);
    PersistAccounts();
  }
}

bool AnarAccountService::PersistAccounts() {
  if (!prefs_ || !encryptor_) {
    return false;
  }
  base::ListValue list;
  for (const auto& account : accounts_) {
    base::DictValue item;
    item.Set("userId", account.info.user_id);
    item.Set("username", account.info.username);
    item.Set("nickname", account.info.nickname);
    item.Set("avatarUrl", account.info.avatar_url);
    item.Set("keys", account.keys_json);
    list.Append(std::move(item));
  }
  const std::string serialized = base::WriteJson(list).value_or("[]");
  std::string ciphertext;
  if (!encryptor_->EncryptString(serialized, &ciphertext)) {
    LOG(ERROR) << "AnarAccountService failed to encrypt account data.";
    return false;
  }
  prefs_->SetString(kAccountsPref, base::Base64Encode(ciphertext));
  return true;
}

bool AnarAccountService::IsSignedIn() const {
  return !accounts_.empty() || pending_account_.has_value();
}

std::string AnarAccountService::GetUserId() const {
  const auto* account = GetPrimaryAccount();
  return account ? account->info.user_id : std::string();
}

std::string AnarAccountService::GetUsername() const {
  const auto* account = GetPrimaryAccount();
  return account ? account->info.username : std::string();
}

std::string AnarAccountService::GetNickname() const {
  const auto* account = GetPrimaryAccount();
  return account ? account->info.nickname : std::string();
}

std::string AnarAccountService::GetAvatarUrl() const {
  const auto* account = GetPrimaryAccount();
  return account ? account->info.avatar_url : std::string();
}

const AnarAccountService::StoredAccount* AnarAccountService::GetPrimaryAccount() const {
  if (!prefs_) return accounts_.empty() ? nullptr : &accounts_.front();
  const auto primary_id = prefs_->GetString(kPrimaryUserIdPref);
  auto it = std::ranges::find_if(accounts_, [&primary_id](const auto& account) {
    return account.info.user_id == primary_id;
  });
  return it == accounts_.end() ? (accounts_.empty() ? nullptr : &accounts_.front()) : &*it;
}

std::vector<AnarAccountInfo> AnarAccountService::GetAccounts() const {
  std::vector<AnarAccountInfo> result;
  result.reserve(accounts_.size());
  const auto primary_id = prefs_ ? prefs_->GetString(kPrimaryUserIdPref) : std::string();
  for (const auto& account : accounts_) {
    auto info = account.info;
    info.is_primary = info.user_id == primary_id ||
                      (primary_id.empty() && &account == &accounts_.front());
    result.push_back(std::move(info));
  }
  return result;
}

bool AnarAccountService::SetPrimaryAccount(const std::string& user_id) {
  if (!prefs_ || std::ranges::none_of(accounts_, [&user_id](const auto& account) {
        return account.info.user_id == user_id;
      })) return false;
  prefs_->SetString(kPrimaryUserIdPref, user_id);
  return true;
}

bool AnarAccountService::RemoveAccount(const std::string& user_id) {
  const auto old_primary = prefs_ ? prefs_->GetString(kPrimaryUserIdPref) : std::string();
  std::erase_if(accounts_, [&user_id](const auto& account) {
    return account.info.user_id == user_id;
  });
  if (accounts_.size() == 0) {
    pending_account_.reset();
    if (prefs_) prefs_->ClearPref(kPrimaryUserIdPref);
  } else if (old_primary == user_id && prefs_) {
    prefs_->SetString(kPrimaryUserIdPref, accounts_.front().info.user_id);
  }
  return PersistAccounts();
}

bool AnarAccountService::SaveSignedInAccount(const std::string& user_id,
                                             const std::string& username,
                                             const std::string& nickname,
                                             const std::string& avatar_url,
                                             const std::string& keys_json) {
  if (!prefs_ || user_id.empty()) {
    return false;
  }
  StoredAccount account;
  account.info = {user_id, username, nickname, avatar_url, false};
  account.keys_json = keys_json;
  if (!encryptor_) {
    pending_account_ = std::move(account);
    return true;
  }
  auto it = std::ranges::find_if(accounts_, [&user_id](const auto& existing) {
    return existing.info.user_id == user_id;
  });
  if (it == accounts_.end()) accounts_.push_back(std::move(account));
  else *it = std::move(account);
  if (prefs_->GetString(kPrimaryUserIdPref).empty())
    prefs_->SetString(kPrimaryUserIdPref, user_id);
  return PersistAccounts();
}

void AnarAccountService::SignOut() {
  pending_account_.reset();
  if (!prefs_) {
    return;
  }
  accounts_.clear();
  prefs_->ClearPref(kPrimaryUserIdPref);
  if (encryptor_) PersistAccounts();
  else prefs_->ClearPref(kAccountsPref);
}

std::string AnarAccountService::GetDecryptedServiceKeysJson() const {
  const auto* account = GetPrimaryAccount();
  return account ? account->keys_json : std::string();
}

std::vector<std::string> AnarAccountService::GetUnlockedServices() const {
  std::vector<std::string> services;
  const std::string keys_json = GetDecryptedServiceKeysJson();
  if (keys_json.empty()) {
    return services;
  }
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(keys_json, base::JSON_PARSE_RFC);
  if (!dict) {
    return services;
  }
  services.reserve(dict->size());
  for (const auto [key, value] : *dict) {
    services.push_back(key);
  }
  return services;
}

}  // namespace anar_account
