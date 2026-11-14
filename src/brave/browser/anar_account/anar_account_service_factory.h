/* Copyright (c) 2026 The Anar Browser Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_ANAR_ACCOUNT_ANAR_ACCOUNT_SERVICE_FACTORY_H_
#define BRAVE_BROWSER_ANAR_ACCOUNT_ANAR_ACCOUNT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace anar_account {

class AnarAccountService;

class AnarAccountServiceFactory : public ProfileKeyedServiceFactory {
 public:
  AnarAccountServiceFactory(const AnarAccountServiceFactory&) = delete;
  AnarAccountServiceFactory& operator=(const AnarAccountServiceFactory&) =
      delete;

  static AnarAccountServiceFactory* GetInstance();
  static AnarAccountService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<AnarAccountServiceFactory>;

  AnarAccountServiceFactory();
  ~AnarAccountServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace anar_account

#endif  // BRAVE_BROWSER_ANAR_ACCOUNT_ANAR_ACCOUNT_SERVICE_FACTORY_H_
