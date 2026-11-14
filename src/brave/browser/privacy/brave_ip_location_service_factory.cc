/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_ip_location_service_factory.h"

#include "brave/browser/privacy/brave_ip_location_service.h"
#include "chrome/browser/profiles/profile.h"

// static
BraveIpLocationServiceFactory* BraveIpLocationServiceFactory::GetInstance() {
  static base::NoDestructor<BraveIpLocationServiceFactory> instance;
  return instance.get();
}

// static
BraveIpLocationService* BraveIpLocationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BraveIpLocationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BraveIpLocationServiceFactory::BraveIpLocationServiceFactory()
    : ProfileKeyedServiceFactory("BraveIpLocationService") {}

BraveIpLocationServiceFactory::~BraveIpLocationServiceFactory() = default;

bool BraveIpLocationServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
BraveIpLocationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<BraveIpLocationService>(
      Profile::FromBrowserContext(context));
}
