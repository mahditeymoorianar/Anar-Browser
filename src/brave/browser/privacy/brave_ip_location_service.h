/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_IP_LOCATION_SERVICE_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_IP_LOCATION_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

// Keeps the fake location and/or fake time zone in step with where the
// browser appears to be on the internet. Also tracks the browser's current
// public IP address (through the profile's own network context, so it sees
// extension-VPN exits as well as system VPNs) for display on the NTP card.
class BraveIpLocationService
    : public KeyedService,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit BraveIpLocationService(Profile* profile);
  BraveIpLocationService(const BraveIpLocationService&) = delete;
  BraveIpLocationService& operator=(const BraveIpLocationService&) = delete;
  ~BraveIpLocationService() override;

  // Returns the most recently observed public IP address, or empty if unknown.
  const std::string& last_ip() const { return last_ip_; }

  // Triggers a public IP lookup if none is cached or the cached value is
  // older than 60 seconds. Never writes fake-location/fake-timezone prefs
  // unless their respective auto-IP prefs are enabled.
  void RefreshPublicIpIfStale();

  // Forces a fresh public IP lookup after a short delay (e.g. when a VPN
  // proxy preference changes).
  void ScheduleIpRefresh();

  // Registers a callback notified whenever `last_ip()` updates.
  base::CallbackListSubscription RegisterIpUpdatedCallback(
      base::RepeatingClosure callback);

  // KeyedService:
  void Shutdown() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  bool IsTimezoneAuto() const;
  bool IsLocationAuto() const;

  void OnPrefsChanged();
  void OnProxyPrefChanged();
  void Refresh(bool force_for_display);
  void OnLookupComplete(std::optional<std::string> response_body);

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;

  base::RepeatingTimer poll_timer_;
  base::OneShotTimer network_change_timer_;

  bool observing_network_ = false;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::string last_ip_;
  base::Time last_lookup_time_;
  base::RepeatingClosureList ip_updated_callbacks_;
  base::WeakPtrFactory<BraveIpLocationService> weak_ptr_factory_{this};
};

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_IP_LOCATION_SERVICE_H_
