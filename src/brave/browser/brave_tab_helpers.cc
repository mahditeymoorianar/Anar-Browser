/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_tab_helpers.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "brave/browser/anar_account/anar_account_service.h"
#include "brave/browser/anar_account/anar_account_service_factory.h"
#include "brave/browser/brave_browser_process.h"
#include "brave/browser/brave_shields/brave_shields_web_contents_observer.h"
#include "brave/browser/ephemeral_storage/ephemeral_storage_tab_helper.h"
#include "brave/browser/misc_metrics/page_metrics_tab_helper.h"
#include "brave/browser/misc_metrics/process_misc_metrics.h"
#include "brave/browser/serp_metrics/serp_metrics_tab_helper.h"
#include "brave/browser/ui/brave_ui_features.h"
#include "brave/components/ai_chat/core/common/buildflags/buildflags.h"
#include "brave/components/brave_ads/buildflags/buildflags.h"
#include "brave/components/brave_news/common/buildflags/buildflags.h"
#include "brave/components/brave_perf_predictor/browser/perf_predictor_tab_helper.h"
#include "brave/components/brave_rewards/core/buildflags/buildflags.h"
#include "brave/components/brave_wallet/common/buildflags/buildflags.h"
#include "brave/components/brave_wayback_machine/buildflags/buildflags.h"
#include "brave/components/playlist/core/common/buildflags/buildflags.h"
#include "brave/components/request_otr/common/buildflags/buildflags.h"
#include "brave/components/serp_metrics/serp_metrics_feature.h"
#include "brave/components/speedreader/common/buildflags/buildflags.h"
#include "brave/components/tor/buildflags/buildflags.h"
#include "brave/components/web_discovery/buildflags/buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_AI_CHAT)
#include "brave/browser/ai_chat/ai_chat_service_factory.h"
#include "brave/browser/ai_chat/ai_chat_utils.h"
#include "brave/components/ai_chat/content/browser/ai_chat_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_BRAVE_ADS)
#include "brave/browser/brave_ads/creatives/search_result_ad/creative_search_result_ad_tab_helper.h"
#include "brave/browser/brave_ads/tabs/ads_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_BRAVE_REWARDS)
#include "brave/browser/brave_rewards/rewards_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PLAYLIST)
#include "brave/browser/playlist/playlist_service_factory.h"
#include "brave/components/playlist/content/browser/playlist_tab_helper.h"
#include "brave/components/playlist/core/browser/utils.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "brave/browser/android/youtube_script_injector/youtube_script_injector_tab_helper.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "brave/browser/brave_shields/brave_shields_tab_helper.h"
#include "brave/browser/privacy/brave_fake_device_info_tab_helper.h"
#include "brave/browser/privacy/brave_voice_swap_tab_helper.h"
#include "brave/browser/ui/geolocation/brave_geolocation_permission_tab_helper.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "brave/browser/new_tab/background_color_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "brave/browser/screenshot/print_preview_extractor_factory.h"
#if BUILDFLAG(ENABLE_AI_CHAT)
#include "brave/browser/ai_chat/print_preview_extraction_delegate_impl.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif  // BUILDFLAG(ENABLE_AI_CHAT)
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "brave/browser/brave_drm_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_BRAVE_WAYBACK_MACHINE)
#include "brave/components/brave_wayback_machine/brave_wayback_machine_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_SPEEDREADER)
#include "brave/browser/ui/speedreader/speedreader_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_TOR)
#include "brave/components/tor/onion_location_tab_helper.h"
#include "brave/components/tor/tor_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_WEB_DISCOVERY)
#include "brave/browser/web_discovery/web_discovery_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_REQUEST_OTR)
#include "brave/browser/request_otr/request_otr_tab_helper.h"
#include "brave/components/request_otr/common/features.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "brave/browser/onboarding/onboarding_tab_helper.h"
#include "brave/browser/ui/sidebar/sidebar_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_BRAVE_WALLET)
#include "brave/browser/brave_wallet/brave_wallet_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_BRAVE_NEWS)
#include "brave/browser/brave_news/brave_news_tab_helper.h"
#endif

namespace brave {

#if BUILDFLAG(IS_WIN)
namespace {

class AnarHandoffTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AnarHandoffTabHelper> {
 public:
  void DidFinishNavigation(content::NavigationHandle* navigation) override {
    const GURL& url = navigation->GetURL();
    if (!navigation->HasCommitted() || navigation->IsErrorPage() ||
        !navigation->IsInPrimaryMainFrame() || navigation->IsSameDocument() ||
        !navigation->IsRendererInitiated() ||
        url::Origin::Create(url) != auth_origin_ || url.path() != "/login" ||
        url.has_ref() || url.has_username() || url.has_password()) {
      return;
    }
    std::string id;
    std::string public_key;
    if (!net::GetValueForKeyInQuery(url, "subsystem", &id) ||
        !net::GetValueForKeyInQuery(url, "pubkey", &public_key) ||
        public_key.size() != 88) {
      return;
    }
    constexpr std::pair<std::string_view, std::string_view> kApps[] = {
        {"notebook", "https://note.anar.one"},
        {"mail", "https://mail.anar.one"},
        {"drive", "https://vault.anar.one"},
        {"calendar", "https://calendar.anar.one"},
        {"contacts", "https://contacts.anar.one"},
        {"voicer", "https://voicer.anar.one"},
        {"tarrah", "https://tarrah.anar.one"},
    };
    GURL app_url;
    for (const auto& [app_id, origin] : kApps) {
      if (id == app_id) {
        app_url = GURL(origin);
        break;
      }
    }
    if (!app_url.is_valid()) {
      return;
    }
    const auto app_origin = url::Origin::Create(app_url);
    if (navigation->GetInitiatorOrigin() != app_origin ||
        url::Origin::Create(navigation->GetPreviousPrimaryMainFrameURL()) !=
            app_origin ||
        (last_app_ == id &&
         base::TimeTicks::Now() - last_attempt_ < base::Minutes(1))) {
      return;
    }
    auto* profile = Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    if (!profile || !profile->IsRegularProfile()) {
      return;
    }
    auto* account = anar_account::AnarAccountServiceFactory::GetForProfile(profile);
    if (!account || !account->IsSignedIn()) {
      return;
    }
    const auto keys = base::JSONReader::ReadDict(
        account->GetDecryptedServiceKeysJson(), base::JSON_PARSE_RFC);
    const auto* key = keys ? keys->FindString(id) : nullptr;
    if (!key || key->size() != 44 || account->GetUsername().empty()) {
      return;
    }
    base::DictValue input;
    input.Set("id", id);
    input.Set("pubkey", public_key);
    input.Set("key", *key);
    input.Set("username", account->GetUsername());
    input.Set("callback", app_url.Resolve("/auth/callback").spec());
    input.Set("login", url.spec());
    const auto json = base::WriteJson(input);
    if (!json) {
      return;
    }
    last_app_ = id;
    last_attempt_ = base::TimeTicks::Now();
    // Match the deployed Anar Auth callback format. The app generated its
    // private key before redirecting; only that app can open this fragment.
    constexpr char kHandoff[] = R"JS(
      (async ({id, pubkey, key, username, callback, login}) => {
        const decode = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));
        const encode = b => btoa(String.fromCharCode(...new Uint8Array(b)));
        const curve = {name: 'ECDH', namedCurve: 'P-256'};
        const peer = await crypto.subtle.importKey(
          'raw', decode(pubkey), curve, false, []);
        const pair = await crypto.subtle.generateKey(curve, false, ['deriveBits']);
        const bits = new Uint8Array(await crypto.subtle.deriveBits(
          {name: 'ECDH', public: peer}, pair.privateKey, 256));
        const encoder = new TextEncoder();
        const ikm = await crypto.subtle.importKey('raw', bits, 'HKDF', false, ['deriveKey']);
        bits.fill(0);
        const sealKey = await crypto.subtle.deriveKey({
          name: 'HKDF', hash: 'SHA-256', salt: new Uint8Array(),
          info: encoder.encode('anar-auth-handoff-v1'),
        }, ikm, {name: 'AES-GCM', length: 256}, false, ['encrypt']);
        const status = await fetch('/api/handoff/status?subsystem=' + id,
          {redirect: 'error', cache: 'no-store'});
        if (!status.ok) return;
        const session = await status.json();
        if (session.username !== username || session.state !== 'ready' ||
            location.href !== login) return;
        const response = await fetch('/api/handoff/complete', {
          method: 'POST', redirect: 'error',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({subsystemId: id, requesterPubkeyB64: pubkey}),
        });
        if (!response.ok) return;
        const {sessionGrant, vaultExists} = await response.json();
        if (!vaultExists || !sessionGrant || location.href !== login) return;
        const iv = crypto.getRandomValues(new Uint8Array(12));
        const payload = encoder.encode(JSON.stringify({
          mkRawBytesB64: key, sessionGrant, remember: true,
        }));
        try {
          const sealed = new Uint8Array(await crypto.subtle.encrypt(
            {name: 'AES-GCM', iv}, sealKey, payload));
          const fragment = encode(await crypto.subtle.exportKey('raw', pair.publicKey)) +
              '.' + encode(new Uint8Array([...iv, ...sealed]));
          if (location.href === login) location.replace(callback + '#' + fragment);
        } finally {
          payload.fill(0);
        }
      })
    )JS";
    navigation->GetRenderFrameHost()->ExecuteJavaScriptInIsolatedWorld(
        base::UTF8ToUTF16(std::string(kHandoff) + "(" + *json + ").catch(() => {});"),
        base::NullCallback(), ISOLATED_WORLD_ID_BRAVE_INTERNAL);
  }

 private:
  friend class content::WebContentsUserData<AnarHandoffTabHelper>;
  explicit AnarHandoffTabHelper(content::WebContents* contents)
      : content::WebContentsObserver(contents), WebContentsUserData(*contents) {}
  const url::Origin auth_origin_ =
      url::Origin::Create(GURL("https://auth.anar.one"));
  std::string last_app_;
  base::TimeTicks last_attempt_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AnarHandoffTabHelper);

}  // namespace
#endif

void AttachTabHelpers(content::WebContents* web_contents) {
  // If your TabHelper is related to privacy consider whether it belongs in this
  // method, so it can also be attached to background WebContents.
  AttachPrivacySensitiveTabHelpers(web_contents);

#if BUILDFLAG(IS_ANDROID)
  YouTubeScriptInjectorTabHelper::CreateForWebContents(web_contents);
#else
  // Add tab helpers here unless they are intended for android too
  brave_shields::BraveShieldsTabHelper::CreateForWebContents(web_contents);
  ThumbnailTabHelper::CreateForWebContents(web_contents);
  BraveGeolocationPermissionTabHelper::CreateForWebContents(web_contents);
  BraveFakeDeviceInfoTabHelper::CreateForWebContents(web_contents);
  BraveVoiceSwapTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_WIN)
  AnarHandoffTabHelper::CreateForWebContents(web_contents);
  if (base::FeatureList::IsEnabled(features::kBraveWorkaroundNewWindowFlash)) {
    BackgroundColorTabHelper::CreateForWebContents(web_contents);
  }
#endif

#if BUILDFLAG(ENABLE_BRAVE_REWARDS)
  brave_rewards::RewardsTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_AI_CHAT)
  content::BrowserContext* context = web_contents->GetBrowserContext();
  if (ai_chat::IsAllowedForContext(context)) {
    ai_chat::AIChatTabHelper::CreateForWebContents(
        web_contents,
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
        std::make_unique<ai_chat::PrintPreviewExtractionDelegateImpl>(
            web_contents,
            screenshot::CreatePrintPreviewExtractor(base::BindRepeating(
                []() -> base::flat_map<base::UnguessableToken, int>& {
                  return printing::PrintPreviewUI::
                      GetPrintPreviewUIRequestIdMap();
                })))
#else
        nullptr
#endif
    );
  }
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
  BraveDrmTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_BRAVE_WAYBACK_MACHINE)
  BraveWaybackMachineTabHelper::CreateIfNeeded(web_contents);
#endif

  brave_perf_predictor::PerfPredictorTabHelper::CreateForWebContents(
      web_contents);

  if (base::FeatureList::IsEnabled(serp_metrics::kSerpMetricsFeature)) {
    serp_metrics::SerpMetricsTabHelper::MaybeCreateForWebContents(web_contents);
  }

#if BUILDFLAG(ENABLE_BRAVE_ADS)
  brave_ads::AdsTabHelper::CreateForWebContents(web_contents);
  brave_ads::CreativeSearchResultAdTabHelper::MaybeCreateForWebContents(
      web_contents);
#endif  // BUILDFLAG(ENABLE_BRAVE_ADS)

#if BUILDFLAG(ENABLE_WEB_DISCOVERY)
  web_discovery::WebDiscoveryTabHelper::MaybeCreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_SPEEDREADER)
  speedreader::SpeedreaderTabHelper::MaybeCreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_TOR)
  tor::TorTabHelper::MaybeCreateForWebContents(web_contents);
  tor::OnionLocationTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_BRAVE_NEWS)
  BraveNewsTabHelper::MaybeCreateForWebContents(web_contents);
#endif

#if defined(TOOLKIT_VIEWS)
  OnboardingTabHelper::MaybeCreateForWebContents(web_contents);
  sidebar::SidebarTabHelper::MaybeCreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_BRAVE_WALLET)
  brave_wallet::BraveWalletTabHelper::CreateForWebContents(web_contents);
#endif

  misc_metrics::PageMetricsTabHelper::CreateForWebContents(web_contents);

#if BUILDFLAG(ENABLE_REQUEST_OTR)
  if (!web_contents->GetBrowserContext()->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(
          request_otr::features::kBraveRequestOTRTab)) {
    RequestOTRTabHelper::CreateForWebContents(web_contents);
  }
#endif

#if BUILDFLAG(ENABLE_PLAYLIST)
  if (playlist::IsPlaylistAllowed(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext()))) {
    if (auto* playlist_service =
            playlist::PlaylistServiceFactory::GetForBrowserContext(
                web_contents->GetBrowserContext())) {
      playlist::PlaylistTabHelper::CreateForWebContents(web_contents,
                                                        playlist_service);
    }
  }
#endif  // BUILDFLAG(ENABLE_PLAYLIST)
}

void AttachPrivacySensitiveTabHelpers(content::WebContents* web_contents) {
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));
  brave_shields::BraveShieldsWebContentsObserver::CreateForWebContents(
      web_contents);
  ephemeral_storage::EphemeralStorageTabHelper::CreateForWebContents(
      web_contents);
}

}  // namespace brave
