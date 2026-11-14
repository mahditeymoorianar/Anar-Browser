/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_TIMEZONE_SERVICE_FACTORY_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_TIMEZONE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BraveFakeTimezoneService;
class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

class BraveFakeTimezoneServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BraveFakeTimezoneServiceFactory* GetInstance();
  static BraveFakeTimezoneService* GetForProfile(Profile* profile);

  BraveFakeTimezoneServiceFactory(const BraveFakeTimezoneServiceFactory&) =
      delete;
  BraveFakeTimezoneServiceFactory& operator=(
      const BraveFakeTimezoneServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BraveFakeTimezoneServiceFactory>;

  BraveFakeTimezoneServiceFactory();
  ~BraveFakeTimezoneServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_TIMEZONE_SERVICE_FACTORY_H_
