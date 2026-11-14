/* Copyright (c) 2026 The Anar Browser Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/anar_account/anar_account_service_factory.h"

#include "brave/browser/anar_account/anar_account_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/user_prefs/user_prefs.h"

namespace anar_account {

// static
AnarAccountServiceFactory* AnarAccountServiceFactory::GetInstance() {
  static base::NoDestructor<AnarAccountServiceFactory> instance;
  return instance.get();
}

// static
AnarAccountService* AnarAccountServiceFactory::GetForProfile(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  return static_cast<AnarAccountService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AnarAccountServiceFactory::AnarAccountServiceFactory()
    : ProfileKeyedServiceFactory(
          "AnarAccountService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {}

AnarAccountServiceFactory::~AnarAccountServiceFactory() = default;

bool AnarAccountServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AnarAccountServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
AnarAccountServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AnarAccountService>(
      user_prefs::UserPrefs::Get(context),
      g_browser_process ? g_browser_process->os_crypt_async() : nullptr);
}

}  // namespace anar_account
