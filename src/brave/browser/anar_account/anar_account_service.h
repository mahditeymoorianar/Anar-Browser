/* Copyright (c) 2026 The Anar Browser Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_ANAR_ACCOUNT_ANAR_ACCOUNT_SERVICE_H_
#define BRAVE_BROWSER_ANAR_ACCOUNT_ANAR_ACCOUNT_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace anar_account {

struct AnarAccountInfo {
  std::string user_id;
  std::string username;
  std::string nickname;
  std::string avatar_url;
  bool is_primary = false;
};

// Stores per-profile Anar Account (anar.one) identity metadata and encrypts
// unlocked service keys using os_crypt_async (Windows DPAPI / App-Bound).
class AnarAccountService : public KeyedService {
 public:
  AnarAccountService(PrefService* prefs,
                     os_crypt_async::OSCryptAsync* os_crypt_async);
  AnarAccountService(const AnarAccountService&) = delete;
  AnarAccountService& operator=(const AnarAccountService&) = delete;
  ~AnarAccountService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  bool IsSignedIn() const;
  std::string GetUserId() const;
  std::string GetUsername() const;
  std::string GetNickname() const;
  std::string GetAvatarUrl() const;
  std::vector<AnarAccountInfo> GetAccounts() const;
  bool SetPrimaryAccount(const std::string& user_id);
  bool RemoveAccount(const std::string& user_id);

  // Encrypts |keys_json| with os_crypt_async before persisting it to profile
  // prefs. Returns false if persistence is unavailable.
  bool SaveSignedInAccount(const std::string& user_id,
                           const std::string& username,
                           const std::string& nickname,
                           const std::string& avatar_url,
                           const std::string& keys_json);

  void SignOut();

  // Returns decrypted service keys JSON for the active profile, or empty
  // string if signed out or decryption fails.
  std::string GetDecryptedServiceKeysJson() const;

  // Returns the list of unlocked Anar service identifiers stored for this
  // profile.
  std::vector<std::string> GetUnlockedServices() const;

 private:
  struct StoredAccount {
    AnarAccountInfo info;
    std::string keys_json;
  };

  void OnEncryptorReady(scoped_refptr<os_crypt_async::Encryptor> encryptor);
  bool PersistAccounts();
  const StoredAccount* GetPrimaryAccount() const;

  raw_ptr<PrefService> prefs_ = nullptr;
  scoped_refptr<os_crypt_async::Encryptor> encryptor_;
  std::vector<StoredAccount> accounts_;
  std::optional<StoredAccount> pending_account_;
  base::WeakPtrFactory<AnarAccountService> weak_factory_{this};
};

}  // namespace anar_account

#endif  // BRAVE_BROWSER_ANAR_ACCOUNT_ANAR_ACCOUNT_SERVICE_H_
