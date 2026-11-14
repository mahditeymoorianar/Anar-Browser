/* Copyright (c) 2026 The Anar Browser Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/anar_account/anar_account_ui.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "base/time/time.h"
#include "brave/browser/anar_account/anar_account_service.h"
#include "brave/browser/anar_account/anar_account_service_factory.h"
#include "brave/components/constants/webui_url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/brave_components_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

constexpr size_t kMaxProxyResponseBytes = 1024 * 1024;  // 1 MiB

constexpr net::NetworkTrafficAnnotationTag kAnarAccountTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("anar_account_auth", R"(
      semantics {
        sender: "Anar Account"
        description:
          "Authenticates the user with their Anar Account (auth.anar.one) "
          "using OPAQUE and retrieves profile and encrypted keyring data."
        trigger:
          "User explicitly submits the Anar Account sign-in or sign-out form."
        data:
          "OPAQUE authentication payloads, session cookies for *.anar.one, "
          "and profile metadata."
        destination: OTHER
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "User-initiated action in anar://anar-account."
        policy_exception_justification: "Only sent on explicit user sign-in."
      })");

bool IsAllowedAnarOrigin(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  constexpr std::string_view kAllowedHosts[] = {
      "auth.anar.one", "account.anar.one", "vault.anar.one",
      "note.anar.one", "mail.anar.one", "calendar.anar.one",
      "tarrah.anar.one", "voicer.anar.one", "contacts.anar.one"};
  for (std::string_view host : kAllowedHosts) {
    if (url.host() == host) {
      return true;
    }
  }
  return false;
}

class AnarAccountHandler : public content::WebUIMessageHandler {
 public:
  AnarAccountHandler() = default;
  AnarAccountHandler(const AnarAccountHandler&) = delete;
  AnarAccountHandler& operator=(const AnarAccountHandler&) = delete;
  ~AnarAccountHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "anarAccountGetOpaqueWasm",
        base::BindRepeating(&AnarAccountHandler::HandleGetOpaqueWasm,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "anarAccountGetState",
        base::BindRepeating(&AnarAccountHandler::HandleGetState,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "anarAccountProxyRequest",
        base::BindRepeating(&AnarAccountHandler::HandleProxyRequest,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "anarAccountSave",
        base::BindRepeating(&AnarAccountHandler::HandleSave,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "anarAccountSignOut",
        base::BindRepeating(&AnarAccountHandler::HandleSignOut,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "anarAccountSetPrimary",
        base::BindRepeating(&AnarAccountHandler::HandleSetPrimary,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "anarAccountRemove",
        base::BindRepeating(&AnarAccountHandler::HandleRemove,
                            base::Unretained(this)));
  }

  void OnJavascriptDisallowed() override { active_loaders_.clear(); }

 private:
  void HandleGetOpaqueWasm(const base::ListValue& args) {
    if (args.empty() || !args[0].is_string()) {
      return;
    }
    AllowJavascript();
    const auto* wasm = ui::ResourceBundle::GetSharedInstance()
                           .LoadDataResourceBytes(
                               IDR_ANAR_ACCOUNT_OPAQUE_WASM_BG);
    if (!wasm) {
      RejectJavascriptCallback(args[0],
                               base::Value("OPAQUE module is unavailable."));
      return;
    }
    const auto bytes = static_cast<base::span<const uint8_t>>(*wasm);
    ResolveJavascriptCallback(args[0], base::Value(base::Base64Encode(bytes)));
  }

  anar_account::AnarAccountService* GetService() {
    Profile* profile = Profile::FromWebUI(web_ui());
    return anar_account::AnarAccountServiceFactory::GetForProfile(profile);
  }

  base::DictValue BuildStateDict() {
    base::DictValue state;
    auto* service = GetService();
    const bool signed_in = service && service->IsSignedIn();
    state.Set("signedIn", signed_in);
    base::ListValue accounts;
    if (service) {
      for (const auto& account : service->GetAccounts()) {
        base::DictValue item;
        item.Set("userId", account.user_id);
        item.Set("username", account.username);
        item.Set("nickname", account.nickname);
        item.Set("avatarUrl", account.avatar_url);
        item.Set("isPrimary", account.is_primary);
        accounts.Append(std::move(item));
      }
    }
    state.Set("accounts", std::move(accounts));
    if (signed_in) {
      state.Set("userId", service->GetUserId());
      state.Set("username", service->GetUsername());
      state.Set("nickname", service->GetNickname());
      state.Set("avatarUrl", service->GetAvatarUrl());
      base::ListValue services;
      for (const auto& svc : service->GetUnlockedServices()) {
        services.Append(svc);
      }
      state.Set("unlockedServices", std::move(services));
    } else {
      state.Set("unlockedServices", base::ListValue());
    }
    return state;
  }

  void HandleGetState(const base::ListValue& args) {
    if (args.empty() || !args[0].is_string()) {
      return;
    }
    AllowJavascript();
    ResolveJavascriptCallback(args[0], BuildStateDict());
  }

  void HandleSave(const base::ListValue& args) {
    if (args.size() < 6 || !args[0].is_string() || !args[1].is_string() ||
        !args[2].is_string() || !args[3].is_string() || !args[4].is_string() ||
        !args[5].is_string()) {
      return;
    }
    AllowJavascript();
    auto* service = GetService();
    if (!service || !service->SaveSignedInAccount(
                        args[1].GetString(), args[2].GetString(),
                        args[3].GetString(), args[4].GetString(),
                        args[5].GetString())) {
      RejectJavascriptCallback(
          args[0], base::Value("Failed to securely encrypt Anar keys."));
      return;
    }
    ResolveJavascriptCallback(args[0], BuildStateDict());
  }

  void HandleSignOut(const base::ListValue& args) {
    if (args.empty() || !args[0].is_string()) {
      return;
    }
    AllowJavascript();
    if (auto* service = GetService()) {
      service->SignOut();
    }
    ResolveJavascriptCallback(args[0], BuildStateDict());
  }

  void HandleSetPrimary(const base::ListValue& args) {
    if (args.size() < 2 || !args[0].is_string() || !args[1].is_string()) return;
    AllowJavascript();
    auto* service = GetService();
    if (!service || !service->SetPrimaryAccount(args[1].GetString())) {
      RejectJavascriptCallback(args[0], base::Value("Account not found."));
      return;
    }
    ResolveJavascriptCallback(args[0], BuildStateDict());
  }

  void HandleRemove(const base::ListValue& args) {
    if (args.size() < 2 || !args[0].is_string() || !args[1].is_string()) return;
    AllowJavascript();
    auto* service = GetService();
    if (!service || !service->RemoveAccount(args[1].GetString())) {
      RejectJavascriptCallback(args[0], base::Value("Could not remove account."));
      return;
    }
    ResolveJavascriptCallback(args[0], BuildStateDict());
  }

  void HandleProxyRequest(const base::ListValue& args) {
    if (args.size() < 4 || !args[0].is_string() || !args[1].is_string() ||
        !args[2].is_string() || !args[3].is_string()) {
      return;
    }
    AllowJavascript();
    const std::string callback_id = args[0].GetString();
    const std::string& method = args[1].GetString();
    const GURL url(args[2].GetString());
    const std::string& body = args[3].GetString();

    if (method != "GET" && method != "POST") {
      base::DictValue err;
      err.Set("ok", false);
      err.Set("status", 400);
      err.Set("body", "{\"detail\":\"Unsupported HTTP method\"}");
      ResolveJavascriptCallback(base::Value(callback_id), err);
      return;
    }

    if (!IsAllowedAnarOrigin(url)) {
      base::DictValue err;
      err.Set("ok", false);
      err.Set("status", 403);
      err.Set("body", "{\"detail\":\"Origin not allowed\"}");
      ResolveJavascriptCallback(base::Value(callback_id), err);
      return;
    }

    Profile* profile = Profile::FromWebUI(web_ui());
    auto url_loader_factory =
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess();
    if (!url_loader_factory) {
      base::DictValue err;
      err.Set("ok", false);
      err.Set("status", 0);
      err.Set("body", "{\"detail\":\"Network stack unavailable\"}");
      ResolveJavascriptCallback(base::Value(callback_id), err);
      return;
    }

    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->method = method;
    request->credentials_mode = network::mojom::CredentialsMode::kInclude;
    request->site_for_cookies = net::SiteForCookies::FromUrl(url);
    request->request_initiator = url::Origin::Create(url);
    request->headers.SetHeader("Accept", "application/json");

    auto loader = network::SimpleURLLoader::Create(
        std::move(request), kAnarAccountTrafficAnnotation);
    loader->SetAllowHttpErrorResults(true);
    if (method == "POST" && !body.empty()) {
      loader->AttachStringForUpload(body, "application/json");
    }

    auto* loader_ptr = loader.get();
    active_loaders_.push_back(std::move(loader));
    loader_ptr->DownloadToString(
        url_loader_factory.get(),
        base::BindOnce(&AnarAccountHandler::OnProxyResponse,
                       weak_factory_.GetWeakPtr(), callback_id, loader_ptr),
        kMaxProxyResponseBytes);
  }

  void OnProxyResponse(const std::string& callback_id,
                       network::SimpleURLLoader* loader_ptr,
                       std::optional<std::string> response_body) {
    int status = 0;
    std::optional<std::string> set_cookie;
    if (loader_ptr->ResponseInfo() && loader_ptr->ResponseInfo()->headers) {
      status = loader_ptr->ResponseInfo()->headers->response_code();
      if (auto header = loader_ptr->ResponseInfo()->headers->EnumerateHeader(
              nullptr, "set-cookie")) {
        set_cookie = std::string(*header);
      }
    }
    const bool ok =
        loader_ptr->NetError() == net::OK && status >= 200 && status < 300;
    const GURL response_url = loader_ptr->GetFinalURL();
    std::erase_if(active_loaders_, [loader_ptr](const auto& item) {
      return item.get() == loader_ptr;
    });

    // Browser-process proxy requests must persist the HttpOnly session cookie
    // in this profile's normal cookie store, just like a request from a web
    // page. Explicitly commit Set-Cookie before resolving the WebUI call so a
    // following navigation immediately sees the new session.
    if (ok && set_cookie && response_url.is_valid()) {
      auto cookie = net::CanonicalCookie::Create(
          response_url, *set_cookie, base::Time::Now(), std::nullopt,
          std::nullopt, net::CookieSourceType::kOther, nullptr);
      if (cookie) {
        auto* cookie_manager = Profile::FromWebUI(web_ui())
                                   ->GetDefaultStoragePartition()
                                   ->GetCookieManagerForBrowserProcess();
        cookie_manager->SetCanonicalCookie(
            *cookie, response_url, net::CookieOptions::MakeAllInclusive(),
            base::BindOnce(&AnarAccountHandler::OnProxyCookieCommitted,
                           weak_factory_.GetWeakPtr(), callback_id, ok, status,
                           response_body.value_or(std::string())));
        return;
      }
    }
    ResolveProxyResponse(callback_id, ok, status,
                         response_body.value_or(std::string()));
  }

  void ResolveProxyResponse(const std::string& callback_id,
                            bool ok,
                            int status,
                            const std::string& body) {
    base::DictValue result;
    result.Set("ok", ok);
    result.Set("status", status);
    result.Set("body", body);
    ResolveJavascriptCallback(base::Value(callback_id), result);
  }

  void OnProxyCookieCommitted(const std::string& callback_id,
                              bool ok,
                              int status,
                              const std::string& body,
                              net::CookieAccessResult cookie_result) {
    ResolveProxyResponse(callback_id,
                         ok && cookie_result.status.IsInclude(), status, body);
  }

  std::vector<std::unique_ptr<network::SimpleURLLoader>> active_loaders_;
  base::WeakPtrFactory<AnarAccountHandler> weak_factory_{this};
};

}  // namespace

AnarAccountUI::AnarAccountUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, kAnarAccountHost);
  source->SetDefaultResource(IDR_ANAR_ACCOUNT_HTML);
  source->AddResourcePath("app.js", IDR_ANAR_ACCOUNT_JS);
  source->AddResourcePath("opaque_wasm.js", IDR_ANAR_ACCOUNT_OPAQUE_WASM_JS);
  source->AddResourcePath("opaque_wasm_bg.wasm",
                          IDR_ANAR_ACCOUNT_OPAQUE_WASM_BG);
  source->AddResourcePath("argon2.umd.min.js", IDR_ANAR_ACCOUNT_ARGON2_JS);
  source->AddResourcePath("anar-logo.png", IDR_ANAR_NTP_LOGO_DARK_BG);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'wasm-unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc, "connect-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src chrome://resources https://account.anar.one data: 'self';");
  source->DisableTrustedTypesCSP();

  web_ui->AddMessageHandler(std::make_unique<AnarAccountHandler>());
}

AnarAccountUI::~AnarAccountUI() = default;
