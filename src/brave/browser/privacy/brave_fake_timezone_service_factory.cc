/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_fake_timezone_service_factory.h"

#include "brave/browser/privacy/brave_fake_timezone_service.h"
#include "chrome/browser/profiles/profile.h"

// static
BraveFakeTimezoneServiceFactory* BraveFakeTimezoneServiceFactory::GetInstance() {
  static base::NoDestructor<BraveFakeTimezoneServiceFactory> instance;
  return instance.get();
}

// static
BraveFakeTimezoneService* BraveFakeTimezoneServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BraveFakeTimezoneService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BraveFakeTimezoneServiceFactory::BraveFakeTimezoneServiceFactory()
    : ProfileKeyedServiceFactory("BraveFakeTimezoneService") {}

BraveFakeTimezoneServiceFactory::~BraveFakeTimezoneServiceFactory() = default;

bool BraveFakeTimezoneServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
BraveFakeTimezoneServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<BraveFakeTimezoneService>(
      Profile::FromBrowserContext(context)->GetPrefs());
}
