/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_TIMEZONE_SERVICE_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_TIMEZONE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

// Watches the "Fake time zone" privacy prefs for a profile and, when
// enabled, pushes a fake IANA time zone id to the browser-process-wide
// device::TimeZoneMonitor so that it is reported to web pages (and to the
// browser itself) instead of the real system time zone. This intentionally
// applies process-wide: the most recently applied profile's setting wins.
class BraveFakeTimezoneService : public KeyedService {
 public:
  explicit BraveFakeTimezoneService(PrefService* profile_prefs);
  BraveFakeTimezoneService(const BraveFakeTimezoneService&) = delete;
  BraveFakeTimezoneService& operator=(const BraveFakeTimezoneService&) =
      delete;
  ~BraveFakeTimezoneService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  void ApplyFakeTimezonePref();

  raw_ptr<PrefService> profile_prefs_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_FAKE_TIMEZONE_SERVICE_H_
