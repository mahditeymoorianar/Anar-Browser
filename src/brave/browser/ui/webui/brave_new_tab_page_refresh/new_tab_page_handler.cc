// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/browser/ui/webui/brave_new_tab_page_refresh/new_tab_page_handler.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/to_address.h"
#include "brave/browser/ntp_background/new_tab_takeover_infobar_delegate.h"
#include "brave/browser/privacy/brave_ip_location_service.h"
#include "brave/browser/privacy/brave_ip_location_service_factory.h"
#include "brave/browser/ui/webui/brave_new_tab_page_refresh/background_facade.h"
#include "brave/browser/ui/webui/brave_new_tab_page_refresh/custom_image_chooser.h"
#include "brave/browser/ui/webui/brave_new_tab_page_refresh/sponsored_sites_facade.h"
#include "brave/browser/ui/webui/brave_new_tab_page_refresh/top_sites_facade.h"
#include "brave/browser/ui/webui/brave_new_tab_page_refresh/vpn_facade.h"
#include "brave/components/brave_ads/buildflags/buildflags.h"
#include "brave/components/brave_perf_predictor/common/pref_names.h"
#include "brave/components/brave_search_conversion/pref_names.h"
#include "brave/components/brave_talk/buildflags/buildflags.h"
#include "brave/components/constants/pref_names.h"
#include "brave/components/constants/url_constants.h"
#include "brave/components/misc_metrics/brave_search_metrics.h"
#include "brave/components/misc_metrics/navigation_source_metrics.h"
#include "brave/components/misc_metrics/new_tab_metrics.h"
#include "brave/components/misc_metrics/page_metrics.h"
#include "brave/components/ntp_background_images/common/pref_names.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <iphlpapi.h>
#endif

#if BUILDFLAG(ENABLE_BRAVE_ADS)
#include "brave/components/brave_ads/core/public/prefs/pref_names.h"
#endif  // BUILDFLAG(ENABLE_BRAVE_ADS)

#if BUILDFLAG(ENABLE_BRAVE_TALK)
#include "brave/components/brave_talk/pref_names.h"
#endif

namespace brave_new_tab_page_refresh {

namespace {

bool IsSponsoredAdsEnabled(const PrefService& pref_service) {
#if BUILDFLAG(ENABLE_BRAVE_ADS)
  return pref_service.GetBoolean(brave_ads::prefs::kSponsoredEnabled);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_BRAVE_ADS)
}

void SetSponsoredAdsEnabled(PrefService& pref_service, bool enabled) {
#if BUILDFLAG(ENABLE_BRAVE_ADS)
  pref_service.SetBoolean(brave_ads::prefs::kSponsoredEnabled, enabled);
#endif  // BUILDFLAG(ENABLE_BRAVE_ADS)
}

constexpr char kProtonVpnExtensionId[] = "jplgfhpmjnbigmhklmmbgecoobifkmpa";

std::string DetectRealSystemTimezone() {
  std::unique_ptr<icu::TimeZone> host_tz(icu::TimeZone::detectHostTimeZone());
  if (!host_tz) {
    return std::string();
  }
  icu::UnicodeString zone_id;
  host_tz->getID(zone_id);
  std::string out;
  zone_id.toUTF8String(out);
  return out;
}

std::string DetectExtensionVpnLabel(Profile* profile,
                                    const PrefService& pref_service) {
  const PrefService::Preference* proxy_pref =
      pref_service.FindPreference(proxy_config::prefs::kProxy);
  if (!proxy_pref || !proxy_pref->IsExtensionControlled()) {
    return std::string();
  }
  const base::DictValue& dict = pref_service.GetDict(proxy_config::prefs::kProxy);
  const std::string* mode = dict.FindString("mode");
  if (!mode || *mode == "direct" || *mode == "system") {
    return std::string();
  }
  if (auto* registry = extensions::ExtensionRegistry::Get(profile)) {
    if (registry->enabled_extensions().Contains(kProtonVpnExtensionId)) {
      return "Proton VPN";
    }
  }
  return "Proxy VPN";
}

std::string DetectSystemVpnAdapterOnBackground() {
#if BUILDFLAG(IS_WIN)
  ULONG buf_len = 16 * 1024;
  std::vector<uint8_t> buffer;
  IP_ADAPTER_ADDRESSES* addresses = nullptr;
  ULONG ret = ERROR_BUFFER_OVERFLOW;
  for (int attempt = 0; attempt < 2 && ret == ERROR_BUFFER_OVERFLOW;
       ++attempt) {
    buffer.resize(buf_len);
    addresses = UNSAFE_BUFFERS(
        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()));
    ret = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, addresses, &buf_len);
  }
  if (ret != NO_ERROR) {
    return std::string();
  }

  for (IP_ADAPTER_ADDRESSES* adapter = addresses; adapter != nullptr;
       adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp) {
      continue;
    }
    const std::string friendly =
        adapter->FriendlyName ? base::WideToUTF8(adapter->FriendlyName)
                              : std::string();
    const std::string desc =
        adapter->Description ? base::WideToUTF8(adapter->Description)
                             : std::string();
    const std::string combined =
        base::ToLowerASCII(friendly + " " + desc);

    const bool is_vpn_type =
        adapter->IfType == IF_TYPE_TUNNEL || adapter->IfType == IF_TYPE_PPP;
    const bool has_vpn_keyword =
        combined.find("mullvad") != std::string::npos ||
        combined.find("proton") != std::string::npos ||
        combined.find("wireguard") != std::string::npos ||
        combined.find("wintun") != std::string::npos ||
        combined.find("openvpn") != std::string::npos ||
        combined.find("tap-windows") != std::string::npos ||
        combined.find("tailscale") != std::string::npos ||
        combined.find("vpn") != std::string::npos;

    // Ignore Windows built-in Teredo/6to4/isatap tunnel pseudo-adapters.
    const bool is_builtin_pseudo =
        combined.find("teredo") != std::string::npos ||
        combined.find("isatap") != std::string::npos ||
        combined.find("6to4") != std::string::npos ||
        combined.find("ip-https") != std::string::npos;

    if ((is_vpn_type || has_vpn_keyword) && !is_builtin_pseudo) {
      if (combined.find("mullvad") != std::string::npos) {
        return "Mullvad VPN";
      }
      if (combined.find("proton") != std::string::npos) {
        return "Proton VPN";
      }
      if (combined.find("wireguard") != std::string::npos) {
        return "WireGuard VPN";
      }
      return !friendly.empty() ? friendly : "System VPN";
    }
  }
#endif
  return std::string();
}

}  // namespace

NewTabPageHandler::NewTabPageHandler(
    mojo::PendingReceiver<mojom::NewTabPageHandler> receiver,
    std::unique_ptr<CustomImageChooser> custom_image_chooser,
    std::unique_ptr<BackgroundFacade> background_facade,
    std::unique_ptr<SponsoredSitesFacade> sponsored_sites_facade,
    std::unique_ptr<TopSitesFacade> top_sites_facade,
    std::unique_ptr<VPNFacade> vpn_facade,
    std::unique_ptr<ChromeAutocompleteSchemeClassifier> scheme_classifier,
    content::WebContents& web_contents,
    PrefService& pref_service,
    TemplateURLService& template_url_service,
    misc_metrics::NewTabMetrics& new_tab_metrics,
    misc_metrics::PageMetrics* page_metrics,
    bool was_restored)
    : receiver_(this, std::move(receiver)),
      update_observer_(pref_service, top_sites_facade.get()),
      custom_image_chooser_(std::move(custom_image_chooser)),
      background_facade_(std::move(background_facade)),
      sponsored_sites_facade_(std::move(sponsored_sites_facade)),
      top_sites_facade_(std::move(top_sites_facade)),
      vpn_facade_(std::move(vpn_facade)),
      scheme_classifier_(std::move(scheme_classifier)),
      web_contents_(web_contents),
      pref_service_(pref_service),
      template_url_service_(template_url_service),
      new_tab_metrics_(new_tab_metrics),
      was_restored_(was_restored) {
  CHECK(custom_image_chooser_);
  CHECK(background_facade_);
  CHECK(sponsored_sites_facade_);
  CHECK(top_sites_facade_);
  CHECK(vpn_facade_);
  CHECK(scheme_classifier_);

  if (page_metrics) {
    brave_search_metrics_ = &page_metrics->brave_search_metrics();
    navigation_source_metrics_ = &page_metrics->navigation_source_metrics();
  }

  update_observer_.SetCallback(base::BindRepeating(&NewTabPageHandler::OnUpdate,
                                                   weak_factory_.GetWeakPtr()));

  sponsored_sites_facade_->SetSitesUpdatedCallback(base::BindRepeating(
      &NewTabPageHandler::OnSponsoredSitesUpdate, weak_factory_.GetWeakPtr()));

  if (auto* profile =
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())) {
    if (auto* ip_service =
            BraveIpLocationServiceFactory::GetForProfile(profile)) {
      ip_updated_subscription_ = ip_service->RegisterIpUpdatedCallback(
          base::BindRepeating(&NewTabPageHandler::OnIpLocationUpdated,
                              weak_factory_.GetWeakPtr()));
    }
    if (auto* settings_map =
            HostContentSettingsMapFactory::GetForProfile(profile)) {
      content_settings_observation_.Observe(settings_map);
    }
  }
}

NewTabPageHandler::~NewTabPageHandler() = default;

void NewTabPageHandler::SetNewTabPage(
    mojo::PendingRemote<mojom::NewTabPage> page) {
  page_.reset();
  page_.Bind(std::move(page));
}

void NewTabPageHandler::GetBackgroundsEnabled(
    GetBackgroundsEnabledCallback callback) {
  bool backgrounds_enabled = pref_service_->GetBoolean(
      ntp_background_images::prefs::kNewTabPageShowBackgroundImage);
  std::move(callback).Run(backgrounds_enabled);
}

void NewTabPageHandler::SetBackgroundsEnabled(
    bool enabled,
    SetBackgroundsEnabledCallback callback) {
  pref_service_->SetBoolean(
      ntp_background_images::prefs::kNewTabPageShowBackgroundImage, enabled);
  std::move(callback).Run();
}

void NewTabPageHandler::GetSponsoredImagesEnabled(
    GetSponsoredImagesEnabledCallback callback) {
  std::move(callback).Run(IsSponsoredAdsEnabled(*pref_service_));
}

void NewTabPageHandler::SetSponsoredImagesEnabled(
    bool enabled,
    SetSponsoredImagesEnabledCallback callback) {
  SetSponsoredAdsEnabled(*pref_service_, enabled);
  std::move(callback).Run();
}

void NewTabPageHandler::GetBraveBackgrounds(
    GetBraveBackgroundsCallback callback) {
  std::move(callback).Run(background_facade_->GetBraveBackgrounds());
}

void NewTabPageHandler::GetCustomBackgrounds(
    GetCustomBackgroundsCallback callback) {
  std::move(callback).Run(background_facade_->GetCustomBackgrounds());
}

void NewTabPageHandler::GetSelectedBackground(
    GetSelectedBackgroundCallback callback) {
  std::move(callback).Run(background_facade_->GetSelectedBackground());
}

void NewTabPageHandler::GetSponsoredImageBackground(
    GetSponsoredImageBackgroundCallback callback) {
  // Do not show sponsored backgrounds for restored tabs.
  // TODO(https://github.com/brave/brave-browser/issues/51359): Use tab
  // visibility instead of `was_restored_` flag.
  if (was_restored_) {
    // Reset `was_restored_` flag so with the next reload the current tab won't
    // be treated as a restored tab.
    was_restored_ = false;
    return std::move(callback).Run(nullptr);
  }

  background_facade_->GetSponsoredImageBackground(
      base::BindOnce(&NewTabPageHandler::OnGetSponsoredImageBackground,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NewTabPageHandler::OnGetSponsoredImageBackground(
    GetSponsoredImageBackgroundCallback callback,
    mojom::SponsoredImageBackgroundPtr sponsored_background) {
  if (sponsored_background) {
    ntp_background_images::NewTabTakeoverInfoBarDelegate::
        MaybeDisplayAndIncrementCounter(base::to_address(web_contents_),
                                        &pref_service_.get());
  }
  std::move(callback).Run(std::move(sponsored_background));
}

void NewTabPageHandler::SelectBackground(
    mojom::SelectedBackgroundPtr background,
    SelectBackgroundCallback callback) {
  background_facade_->SelectBackground(std::move(background));
  std::move(callback).Run();
}

void NewTabPageHandler::ShowCustomBackgroundChooser(
    ShowCustomBackgroundChooserCallback callback) {
  custom_image_chooser_->ShowDialog(
      base::BindOnce(&NewTabPageHandler::OnCustomBackgroundsSelected,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NewTabPageHandler::RemoveCustomBackground(
    const std::string& background_url,
    RemoveCustomBackgroundCallback callback) {
  background_facade_->RemoveCustomBackground(background_url,
                                             std::move(callback));
}

void NewTabPageHandler::NotifySponsoredImageLogoClicked(
    const std::string& wallpaper_id,
    const std::string& creative_instance_id,
    const std::string& destination_url,
    brave_ads::mojom::NewTabPageAdMetricType mojom_ad_metric_type,
    NotifySponsoredImageLogoClickedCallback callback) {
  background_facade_->NotifySponsoredImageLogoClicked(
      wallpaper_id, creative_instance_id, destination_url,
      mojom_ad_metric_type);
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowSearchBox(GetShowSearchBoxCallback callback) {
  std::move(callback).Run(pref_service_->GetBoolean(
      brave_search_conversion::prefs::kShowNTPSearchBox));
}

void NewTabPageHandler::SetShowSearchBox(bool show_search_box,
                                         SetShowSearchBoxCallback callback) {
  pref_service_->SetBoolean(brave_search_conversion::prefs::kShowNTPSearchBox,
                            show_search_box);
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowChatInput(GetShowChatInputCallback callback) {
  std::move(callback).Run(pref_service_->GetBoolean(
      brave_search_conversion::prefs::kShowNTPChatInput));
}

void NewTabPageHandler::SetShowChatInput(bool show_chat_input,
                                         SetShowChatInputCallback callback) {
  pref_service_->SetBoolean(brave_search_conversion::prefs::kShowNTPChatInput,
                            show_chat_input);
  std::move(callback).Run();
}

void NewTabPageHandler::GetSearchSuggestionsEnabled(
    GetSearchSuggestionsEnabledCallback callback) {
  std::move(callback).Run(
      pref_service_->GetBoolean(prefs::kSearchSuggestEnabled));
}

void NewTabPageHandler::SetSearchSuggestionsEnabled(
    bool enabled,
    SetSearchSuggestionsEnabledCallback callback) {
  pref_service_->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
  std::move(callback).Run();
}

void NewTabPageHandler::GetSearchSuggestionsPromptDismissed(
    GetSearchSuggestionsPromptDismissedCallback callback) {
  std::move(callback).Run(
      pref_service_->GetBoolean(brave_search_conversion::prefs::kDismissed));
}

void NewTabPageHandler::SetSearchSuggestionsPromptDismissed(
    bool dismissed,
    SetSearchSuggestionsPromptDismissedCallback callback) {
  pref_service_->SetBoolean(brave_search_conversion::prefs::kDismissed,
                            dismissed);
  std::move(callback).Run();
}

void NewTabPageHandler::GetLastUsedSearchEngine(
    GetLastUsedSearchEngineCallback callback) {
  std::move(callback).Run(pref_service_->GetString(
      brave_search_conversion::prefs::kLastUsedNTPSearchEngine));
}

void NewTabPageHandler::SetLastUsedSearchEngine(
    const std::string& engine_host,
    SetLastUsedSearchEngineCallback callback) {
  pref_service_->SetString(
      brave_search_conversion::prefs::kLastUsedNTPSearchEngine, engine_host);
  std::move(callback).Run();
}

void NewTabPageHandler::GetAvailableSearchEngines(
    GetAvailableSearchEnginesCallback callback) {
  std::vector<mojom::SearchEngineInfoPtr> search_engines;
  for (auto template_url : template_url_service_->GetTemplateURLs()) {
    if (template_url->GetBuiltinEngineType() !=
        BuiltinEngineType::KEYWORD_MODE_PREPOPULATED_ENGINE) {
      continue;
    }
    const GURL url(template_url->GenerateSearchURL(
        template_url_service_->search_terms_data()));
    if (!url.is_valid() || !url.has_host()) {
      continue;
    }
    auto search_engine = mojom::SearchEngineInfo::New();
    search_engine->prepopulate_id = template_url->prepopulate_id();
    search_engine->host = url.host();
    search_engine->name = base::UTF16ToUTF8(template_url->short_name());
    search_engine->keyword = base::UTF16ToUTF8(template_url->keyword());
    search_engine->favicon_url = template_url->favicon_url().spec();
    search_engines.push_back(std::move(search_engine));
  }
  std::move(callback).Run(std::move(search_engines));
}

void NewTabPageHandler::OpenSearch(const std::string& query,
                                   const std::string& engine,
                                   mojom::EventDetailsPtr details,
                                   OpenSearchCallback callback) {
  auto* template_url = template_url_service_->GetTemplateURLForHost(engine);
  if (!template_url) {
    std::move(callback).Run();
    return;
  }

  GURL search_url = template_url->GenerateSearchURL(
      template_url_service_->search_terms_data(), base::UTF8ToUTF16(query));

  OpenGURL(search_url,
           ui::DispositionFromClick(false, details->alt_key, details->ctrl_key,
                                    details->meta_key, details->shift_key));

  std::move(callback).Run();
}

void NewTabPageHandler::OpenURLFromSearch(const std::string& url,
                                          mojom::EventDetailsPtr details,
                                          OpenURLFromSearchCallback callback) {
  OpenGURL(GURL(url),
           ui::DispositionFromClick(false, details->alt_key, details->ctrl_key,
                                    details->meta_key, details->shift_key));
  std::move(callback).Run();
}

void NewTabPageHandler::GetUrlFromSearchInput(
    const std::string& input,
    GetUrlFromSearchInputCallback callback) {
  AutocompleteInput autocomplete_input(
      base::UTF8ToUTF16(input), metrics::OmniboxEventProto::NTP_REALBOX,
      *scheme_classifier_,
      /*should_use_https_as_default_scheme=*/true);
  if (autocomplete_input.type() != metrics::OmniboxInputType::URL ||
      !autocomplete_input.canonicalized_url().is_valid()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(autocomplete_input.canonicalized_url().spec());
}

void NewTabPageHandler::SetDefaultSearchEngineAsBraveSearch(
    SetDefaultSearchEngineAsBraveSearchCallback callback) {
  if (auto* template_url =
          template_url_service_->GetTemplateURLForHost(kBraveSearchHost)) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }
  std::move(callback).Run();
}

void NewTabPageHandler::ReportSearchBoxHidden(
    ReportSearchBoxHiddenCallback callback) {
  new_tab_metrics_->ReportNTPSearchDefaultEngine(std::nullopt);
  std::move(callback).Run();
}

void NewTabPageHandler::ReportSearchEngineUsage(
    int64_t engine_prepopulate_id,
    ReportSearchEngineUsageCallback callback) {
  new_tab_metrics_->ReportNTPSearchDefaultEngine(engine_prepopulate_id);
  std::move(callback).Run();
}

void NewTabPageHandler::ReportSearchResultUsage(
    int64_t engine_prepopulate_id,
    ReportSearchResultUsageCallback callback) {
  new_tab_metrics_->ReportNTPSearchUsage(engine_prepopulate_id);
  if (brave_search_metrics_) {
    brave_search_metrics_->MaybeRecordNTPSearch(engine_prepopulate_id);
  }
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowTopSites(GetShowTopSitesCallback callback) {
  std::move(callback).Run(top_sites_facade_->GetTopSitesVisible());
}

void NewTabPageHandler::SetShowTopSites(bool show_top_sites,
                                        SetShowTopSitesCallback callback) {
  top_sites_facade_->SetTopSitesVisible(show_top_sites);
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowSponsoredSites(
    GetShowSponsoredSitesCallback callback) {
  std::move(callback).Run(IsSponsoredAdsEnabled(*pref_service_));
}

void NewTabPageHandler::SetShowSponsoredSites(
    bool show_sponsored_sites,
    SetShowSponsoredSitesCallback callback) {
  SetSponsoredAdsEnabled(*pref_service_, show_sponsored_sites);
  std::move(callback).Run();
}

void NewTabPageHandler::GetTopSitesListKind(
    GetTopSitesListKindCallback callback) {
  std::move(callback).Run(top_sites_facade_->GetListKind());
}

void NewTabPageHandler::SetTopSitesListKind(
    mojom::TopSitesListKind list_kind,
    SetTopSitesListKindCallback callback) {
  top_sites_facade_->SetListKind(list_kind);
  std::move(callback).Run();
}

void NewTabPageHandler::GetTopSites(GetTopSitesCallback callback) {
  top_sites_facade_->GetSites(std::move(callback));
}

void NewTabPageHandler::AddCustomTopSite(const std::string& url,
                                         const std::string& title,
                                         AddCustomTopSiteCallback callback) {
  top_sites_facade_->AddCustomSite(url, title);
  std::move(callback).Run();
}

void NewTabPageHandler::UpdateCustomTopSite(
    const std::string& url,
    const std::string& new_url,
    const std::string& title,
    UpdateCustomTopSiteCallback callback) {
  top_sites_facade_->UpdateCustomSite(url, new_url, title);
  std::move(callback).Run();
}

void NewTabPageHandler::RemoveCustomTopSite(
    const std::string& url,
    RemoveCustomTopSiteCallback callback) {
  top_sites_facade_->RemoveCustomSite(url);
  std::move(callback).Run();
}

void NewTabPageHandler::UndoCustomTopSiteAction(
    UndoCustomTopSiteActionCallback callback) {
  top_sites_facade_->UndoCustomSiteAction();
  std::move(callback).Run();
}

void NewTabPageHandler::ExcludeMostVisitedTopSite(
    const std::string& url,
    ExcludeMostVisitedTopSiteCallback callback) {
  top_sites_facade_->ExcludeMostVisitedSite(url);
  std::move(callback).Run();
}

void NewTabPageHandler::IncludeMostVisitedTopSite(
    const std::string& url,
    IncludeMostVisitedTopSiteCallback callback) {
  top_sites_facade_->IncludeMostVisitedTopSite(url);
  std::move(callback).Run();
}

void NewTabPageHandler::RecordTopSiteClick(
    RecordTopSiteClickCallback callback) {
  if (navigation_source_metrics_) {
    bool is_custom =
        top_sites_facade_->GetListKind() == mojom::TopSitesListKind::kCustom;
    navigation_source_metrics_->RecordTopSiteNavigation(is_custom);
  }
  std::move(callback).Run();
}

void NewTabPageHandler::GetSponsoredSites(GetSponsoredSitesCallback callback) {
  sponsored_sites_facade_->GetSites(std::move(callback));
}

void NewTabPageHandler::SetCustomTopSitePosition(
    const std::string& url,
    int32_t position,
    SetCustomTopSitePositionCallback callback) {
  top_sites_facade_->SetCustomSitePosition(url, position);
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowClock(GetShowClockCallback callback) {
  std::move(callback).Run(pref_service_->GetBoolean(kNewTabPageShowClock));
}

void NewTabPageHandler::SetShowClock(bool show_clock,
                                     SetShowClockCallback callback) {
  pref_service_->SetBoolean(kNewTabPageShowClock, show_clock);
  std::move(callback).Run();
}

void NewTabPageHandler::GetClockFormat(GetClockFormatCallback callback) {
  auto to_clock_format = [](const std::string& format_string) {
    if (format_string == "h12") {
      return mojom::ClockFormat::k12;
    } else if (format_string == "h24") {
      return mojom::ClockFormat::k24;
    } else {
      return mojom::ClockFormat::kAuto;
    }
  };

  std::move(callback).Run(
      to_clock_format(pref_service_->GetString(kNewTabPageClockFormat)));
}

void NewTabPageHandler::SetClockFormat(mojom::ClockFormat clock_format,
                                       SetClockFormatCallback callback) {
  auto stringify_format = [](mojom::ClockFormat format) {
    switch (format) {
      case mojom::ClockFormat::k12:
        return "h12";
      case mojom::ClockFormat::k24:
        return "h24";
      case mojom::ClockFormat::kAuto:
        return "";
    }
  };
  pref_service_->SetString(kNewTabPageClockFormat,
                           stringify_format(clock_format));
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowShieldsStats(
    GetShowShieldsStatsCallback callback) {
  std::move(callback).Run(pref_service_->GetBoolean(kNewTabPageShowStats));
}

void NewTabPageHandler::SetShowShieldsStats(
    bool show_shields_stats,
    SetShowShieldsStatsCallback callback) {
  pref_service_->SetBoolean(kNewTabPageShowStats, show_shields_stats);
  std::move(callback).Run();
}

void NewTabPageHandler::GetShieldsStats(GetShieldsStatsCallback callback) {
  auto stats = mojom::ShieldsStats::New();
  stats->ads_blocked = pref_service_->GetUint64(kAdsBlocked) +
                       pref_service_->GetUint64(kTrackersBlocked);
  stats->bandwidth_saved_bytes = pref_service_->GetUint64(
      brave_perf_predictor::prefs::kBandwidthSavedBytes);
  std::move(callback).Run(std::move(stats));
}

void NewTabPageHandler::GetPrivacyStatus(GetPrivacyStatusCallback callback) {
  auto status = mojom::PrivacyStatus::New();

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile) {
    if (auto* ip_service =
            BraveIpLocationServiceFactory::GetForProfile(profile)) {
      status->ip_address = ip_service->last_ip();
      ip_service->RefreshPublicIpIfStale();
    }
  }

  status->fake_timezone_enabled =
      pref_service_->GetBoolean(kFakeTimezoneEnabled);
  status->fake_timezone_id = pref_service_->GetString(kFakeTimezoneId);
  status->fake_timezone_auto_ip =
      pref_service_->GetBoolean(kFakeTimezoneAutoIp);
  status->real_timezone_id = DetectRealSystemTimezone();

  ContentSetting geo_setting = CONTENT_SETTING_ASK;
  if (profile) {
    if (auto* settings_map =
            HostContentSettingsMapFactory::GetForProfile(profile)) {
      geo_setting = settings_map->GetDefaultContentSetting(
          ContentSettingsType::GEOLOCATION);
    }
  }
  const bool fake_geo_enabled =
      pref_service_->GetBoolean(kFakeGeolocationEnabled);
  if (geo_setting == CONTENT_SETTING_BLOCK) {
    status->geolocation_mode = mojom::GeolocationMode::kBlocked;
  } else if (fake_geo_enabled) {
    status->geolocation_mode = mojom::GeolocationMode::kFake;
  } else {
    status->geolocation_mode = mojom::GeolocationMode::kReal;
  }
  status->fake_latitude = pref_service_->GetDouble(kFakeGeolocationLatitude);
  status->fake_longitude = pref_service_->GetDouble(kFakeGeolocationLongitude);
  status->fake_geolocation_auto_ip =
      pref_service_->GetBoolean(kFakeGeolocationAutoIp);

  std::string ext_vpn =
      profile ? DetectExtensionVpnLabel(profile, pref_service_.get())
              : std::string();
  if (!ext_vpn.empty()) {
    status->vpn_connected = true;
    status->vpn_status = "Connected (" + ext_vpn + ")";
    std::move(callback).Run(std::move(status));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DetectSystemVpnAdapterOnBackground),
      base::BindOnce(&NewTabPageHandler::OnPrivacyStatusVpnChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(status)));
}

void NewTabPageHandler::OnPrivacyStatusVpnChecked(
    GetPrivacyStatusCallback callback,
    mojom::PrivacyStatusPtr status,
    std::string vpn_label) {
  if (!vpn_label.empty()) {
    status->vpn_connected = true;
    status->vpn_status = "Connected (" + vpn_label + ")";
  } else {
    status->vpn_connected = false;
    status->vpn_status = "Disconnected";
  }
  std::move(callback).Run(std::move(status));
}

#if BUILDFLAG(ENABLE_BRAVE_TALK)
void NewTabPageHandler::GetShowTalkWidget(GetShowTalkWidgetCallback callback) {
  std::move(callback).Run(
      pref_service_->GetBoolean(brave_talk::prefs::kNewTabPageShowBraveTalk));
}

void NewTabPageHandler::SetShowTalkWidget(bool show_talk_widget,
                                          SetShowTalkWidgetCallback callback) {
  pref_service_->SetBoolean(brave_talk::prefs::kNewTabPageShowBraveTalk,
                            show_talk_widget);
  std::move(callback).Run();
}
#endif  // BUILDFLAG(ENABLE_BRAVE_TALK)

void NewTabPageHandler::GetShowRewardsWidget(
    GetShowRewardsWidgetCallback callback) {
  std::move(callback).Run(pref_service_->GetBoolean(kNewTabPageShowRewards));
}

void NewTabPageHandler::SetShowRewardsWidget(
    bool show_rewards_widget,
    SetShowRewardsWidgetCallback callback) {
  pref_service_->SetBoolean(kNewTabPageShowRewards, show_rewards_widget);
  std::move(callback).Run();
}

void NewTabPageHandler::GetShowVPNWidget(GetShowVPNWidgetCallback callback) {
  if (std::optional<std::string> pref_name = vpn_facade_->GetWidgetPrefName()) {
    std::move(callback).Run(pref_service_->GetBoolean(*pref_name));
  } else {
    std::move(callback).Run(false);
  }
}

void NewTabPageHandler::SetShowVPNWidget(bool show_vpn_widget,
                                         SetShowVPNWidgetCallback callback) {
  if (std::optional<std::string> pref_name = vpn_facade_->GetWidgetPrefName()) {
    pref_service_->SetBoolean(*pref_name, show_vpn_widget);
  }
  std::move(callback).Run();
}

void NewTabPageHandler::ReloadVPNPurchasedState(
    ReloadVPNPurchasedStateCallback callback) {
  vpn_facade_->ReloadPurchasedState();
  std::move(callback).Run();
}

void NewTabPageHandler::OpenVPNPanel(OpenVPNPanelCallback callback) {
  vpn_facade_->OpenPanel();
  std::move(callback).Run();
}

void NewTabPageHandler::OpenVPNAccountPage(
    brave_vpn::mojom::ManageURLType url_type,
    OpenVPNAccountPageCallback callback) {
  vpn_facade_->OpenAccountPage(url_type);
  std::move(callback).Run();
}

void NewTabPageHandler::ReportVPNWidgetUsage(
    ReportVPNWidgetUsageCallback callback) {
  vpn_facade_->RecordWidgetUsage();
  std::move(callback).Run();
}

void NewTabPageHandler::OnCustomBackgroundsSelected(
    ShowCustomBackgroundChooserCallback callback,
    std::vector<base::FilePath> paths) {
  // Before continuing, notify the caller of whether backgrounds were selected.
  // This allows the front-end to display a loading indicator while the save
  // operation is in progress.
  std::move(callback).Run(!paths.empty());

  if (!paths.empty()) {
    background_facade_->SaveCustomBackgrounds(std::move(paths),
                                              base::DoNothing());
  }
}

void NewTabPageHandler::OnUpdate(UpdateObserver::Source update_source) {
  if (!page_.is_bound()) {
    return;
  }
  switch (update_source) {
    case UpdateObserver::Source::kBackgrounds:
      page_->OnBackgroundsUpdated();
      break;
    case UpdateObserver::Source::kSearch:
      page_->OnSearchStateUpdated();
      break;
    case UpdateObserver::Source::kTopSites:
      page_->OnTopSitesUpdated();
      page_->OnSponsoredSitesUpdated();
      break;
    case UpdateObserver::Source::kClock:
      page_->OnClockStateUpdated();
      break;
    case UpdateObserver::Source::kShieldsStats:
      page_->OnShieldsStatsUpdated();
      break;
    case UpdateObserver::Source::kPrivacyStatus:
      page_->OnPrivacyStatusUpdated();
      break;
#if BUILDFLAG(ENABLE_BRAVE_TALK)
    case UpdateObserver::Source::kTalk:
      page_->OnTalkStateUpdated();
      break;
#endif
    case UpdateObserver::Source::kRewards:
      page_->OnRewardsStateUpdated();
      break;
    case UpdateObserver::Source::kVPN:
      page_->OnVPNStateUpdated();
      break;
  }
}

void NewTabPageHandler::OnSponsoredSitesUpdate() {
  if (page_.is_bound()) {
    page_->OnSponsoredSitesUpdated();
  }
}

void NewTabPageHandler::OnIpLocationUpdated() {
  if (page_.is_bound()) {
    page_->OnPrivacyStatusUpdated();
  }
}

void NewTabPageHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (page_.is_bound() &&
      content_type_set.Contains(ContentSettingsType::GEOLOCATION)) {
    page_->OnPrivacyStatusUpdated();
  }
}

void NewTabPageHandler::OpenGURL(const GURL& gurl,
                                 WindowOpenDisposition disposition) {
  if (auto* tab = tabs::TabInterface::MaybeGetFromContents(
          base::to_address(web_contents_))) {
    tab->GetBrowserWindowInterface()->OpenGURL(gurl, disposition);
  }
}

}  // namespace brave_new_tab_page_refresh
