/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_fake_timezone_service.h"

#include "base/functional/bind.h"
#include "brave/components/constants/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"

BraveFakeTimezoneService::BraveFakeTimezoneService(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  pref_change_registrar_.Init(profile_prefs_);
  auto callback = base::BindRepeating(
      &BraveFakeTimezoneService::ApplyFakeTimezonePref,
      base::Unretained(this));
  pref_change_registrar_.Add(kFakeTimezoneEnabled, callback);
  pref_change_registrar_.Add(kFakeTimezoneId, callback);
  ApplyFakeTimezonePref();
}

BraveFakeTimezoneService::~BraveFakeTimezoneService() = default;

void BraveFakeTimezoneService::Shutdown() {
  pref_change_registrar_.Reset();
}

void BraveFakeTimezoneService::ApplyFakeTimezonePref() {
  mojo::Remote<device::mojom::TimeZoneMonitor> time_zone_monitor;
  content::GetDeviceService().BindTimeZoneMonitor(
      time_zone_monitor.BindNewPipeAndPassReceiver());

  if (profile_prefs_->GetBoolean(kFakeTimezoneEnabled)) {
    const std::string zone_id = profile_prefs_->GetString(kFakeTimezoneId);
    if (!zone_id.empty()) {
      time_zone_monitor->SetTimeZoneOverride(zone_id);
      return;
    }
  }
  time_zone_monitor->ClearTimeZoneOverride();
}
