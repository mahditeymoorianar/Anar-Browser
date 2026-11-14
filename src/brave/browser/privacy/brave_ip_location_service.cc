/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_ip_location_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "brave/components/constants/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

constexpr char kLookupUrl[] = "https://ipwho.is/";
constexpr base::TimeDelta kPollInterval = base::Minutes(5);
constexpr base::TimeDelta kNetworkChangeDelay = base::Seconds(3);
constexpr base::TimeDelta kDisplayStaleThreshold = base::Seconds(60);
constexpr size_t kMaxResponseSize = 64 * 1024;

}  // namespace

BraveIpLocationService::BraveIpLocationService(Profile* profile)
    : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  auto callback = base::BindRepeating(&BraveIpLocationService::OnPrefsChanged,
                                      base::Unretained(this));
  pref_change_registrar_.Add(kFakeGeolocationEnabled, callback);
  pref_change_registrar_.Add(kFakeGeolocationAutoIp, callback);
  pref_change_registrar_.Add(kFakeTimezoneEnabled, callback);
  pref_change_registrar_.Add(kFakeTimezoneAutoIp, callback);
  pref_change_registrar_.Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&BraveIpLocationService::OnProxyPrefChanged,
                          base::Unretained(this)));
  OnPrefsChanged();
}

BraveIpLocationService::~BraveIpLocationService() = default;

void BraveIpLocationService::Shutdown() {
  pref_change_registrar_.Reset();
  poll_timer_.Stop();
  network_change_timer_.Stop();
  loader_.reset();
  if (observing_network_) {
    if (auto* tracker = content::GetNetworkConnectionTracker()) {
      tracker->RemoveNetworkConnectionObserver(this);
    }
    observing_network_ = false;
  }
}

base::CallbackListSubscription BraveIpLocationService::RegisterIpUpdatedCallback(
    base::RepeatingClosure callback) {
  if (!observing_network_) {
    if (auto* tracker = content::GetNetworkConnectionTracker()) {
      tracker->AddNetworkConnectionObserver(this);
      observing_network_ = true;
    }
  }
  return ip_updated_callbacks_.Add(std::move(callback));
}

void BraveIpLocationService::RefreshPublicIpIfStale() {
  if (!last_ip_.empty() && !last_lookup_time_.is_null() &&
      (base::Time::Now() - last_lookup_time_) < kDisplayStaleThreshold) {
    return;
  }
  Refresh(/*force_for_display=*/true);
}

void BraveIpLocationService::ScheduleIpRefresh() {
  network_change_timer_.Start(
      FROM_HERE, kNetworkChangeDelay,
      base::BindOnce(&BraveIpLocationService::Refresh, base::Unretained(this),
                     /*force_for_display=*/true));
}

bool BraveIpLocationService::IsTimezoneAuto() const {
  auto* prefs = profile_->GetPrefs();
  return prefs->GetBoolean(kFakeTimezoneEnabled) &&
         prefs->GetBoolean(kFakeTimezoneAutoIp);
}

bool BraveIpLocationService::IsLocationAuto() const {
  auto* prefs = profile_->GetPrefs();
  return prefs->GetBoolean(kFakeGeolocationEnabled) &&
         prefs->GetBoolean(kFakeGeolocationAutoIp);
}

void BraveIpLocationService::OnProxyPrefChanged() {
  if (!ip_updated_callbacks_.empty() || IsTimezoneAuto() || IsLocationAuto()) {
    ScheduleIpRefresh();
  }
}

void BraveIpLocationService::OnPrefsChanged() {
  const bool active = IsTimezoneAuto() || IsLocationAuto();
  if (!active) {
    poll_timer_.Stop();
    if (ip_updated_callbacks_.empty()) {
      network_change_timer_.Stop();
      loader_.reset();
      if (observing_network_) {
        if (auto* tracker = content::GetNetworkConnectionTracker()) {
          tracker->RemoveNetworkConnectionObserver(this);
        }
        observing_network_ = false;
      }
    }
    return;
  }

  if (!observing_network_) {
    if (auto* tracker = content::GetNetworkConnectionTracker()) {
      tracker->AddNetworkConnectionObserver(this);
      observing_network_ = true;
    }
  }
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(
        FROM_HERE, kPollInterval,
        base::BindRepeating(&BraveIpLocationService::Refresh,
                            base::Unretained(this),
                            /*force_for_display=*/false));
  }
  Refresh(/*force_for_display=*/false);
}

void BraveIpLocationService::OnConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (ip_updated_callbacks_.empty() && !IsTimezoneAuto() && !IsLocationAuto()) {
    return;
  }
  ScheduleIpRefresh();
}

void BraveIpLocationService::Refresh(bool force_for_display) {
  if (loader_) {
    return;
  }
  if (!force_for_display && !IsTimezoneAuto() && !IsLocationAuto()) {
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("anar_ip_location_lookup", R"(
        semantics {
          sender: "Anar Privacy Status"
          description:
            "Looks up the browser's current public IP address (and, when "
            "enabled in Privacy Settings, approximate location and time zone) "
            "for display on the New Tab page and automatic VPN-exit matching."
          trigger:
            "When opening a New Tab page (if cached IP is stale) or when "
            "'match my IP location' is enabled."
          data:
            "None. Plain GET request with no cookies or credentials."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "Controlled by Anar New Tab / Privacy Settings."
          policy_exception_justification: "Not implemented."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kLookupUrl);
  request->method = "GET";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;

  loader_ = network::SimpleURLLoader::Create(std::move(request),
                                             traffic_annotation);
  loader_->SetTimeoutDuration(base::Seconds(15));
  loader_->DownloadToString(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&BraveIpLocationService::OnLookupComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxResponseSize);
}

void BraveIpLocationService::OnLookupComplete(
    std::optional<std::string> response_body) {
  loader_.reset();
  if (!response_body) {
    return;
  }

  std::optional<base::DictValue> root =
      base::JSONReader::ReadDict(*response_body, base::JSON_PARSE_RFC);
  if (!root) {
    return;
  }
  if (root->FindBool("success").value_or(false) == false) {
    return;
  }

  if (const std::string* ip = root->FindString("ip"); ip && !ip->empty()) {
    last_ip_ = *ip;
    last_lookup_time_ = base::Time::Now();
    ip_updated_callbacks_.Notify();
  }

  auto* prefs = profile_->GetPrefs();

  if (IsLocationAuto()) {
    const std::optional<double> latitude = root->FindDouble("latitude");
    const std::optional<double> longitude = root->FindDouble("longitude");
    if (latitude && longitude && *latitude >= -90.0 && *latitude <= 90.0 &&
        *longitude >= -180.0 && *longitude <= 180.0) {
      prefs->SetDouble(kFakeGeolocationLatitude, *latitude);
      prefs->SetDouble(kFakeGeolocationLongitude, *longitude);
    }
  }

  if (IsTimezoneAuto()) {
    const base::DictValue* timezone = root->FindDict("timezone");
    const std::string* zone_id =
        timezone ? timezone->FindString("id") : nullptr;
    if (zone_id && !zone_id->empty() &&
        (zone_id->find('/') != std::string::npos || *zone_id == "UTC")) {
      prefs->SetString(kFakeTimezoneId, *zone_id);
    }
  }
}
