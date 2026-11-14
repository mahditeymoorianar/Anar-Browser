/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/profiles/brave_profile_menu_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "brave/browser/profiles/profile_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/browser/anar_account/anar_account_service.h"
#include "brave/browser/anar_account/anar_account_service_factory.h"
#include "brave/components/constants/webui_url_constants.h"
#include "brave/components/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

void BraveProfileMenuView::MaybeBuildCloseBrowsersButton() {
  Profile* profile = browser().GetProfile();
  int window_count =
      ProfileBrowserCollection::GetForProfile(profile)->GetSize();
  if (!profile->IsOffTheRecord() && profile->HasPrimaryOTRProfile()) {
    window_count +=
        ProfileBrowserCollection::GetForProfile(
            profile->GetPrimaryOTRProfile(/*create_if_needed=*/true))
            ->GetSize();
  }

  int button_title_id = IDS_PROFILE_MENU_CLOSE_PROFILE_X_WINDOWS_BUTTON;
  if (profile->IsGuestSession()) {
    button_title_id = IDS_GUEST_PROFILE_MENU_CLOSE_X_WINDOWS_BUTTON;
  } else {
    if (window_count <= 1) {
      return;
    }
  }

  AddFeatureButton(
      l10n_util::GetPluralStringFUTF16(button_title_id, window_count),
      base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                          base::Unretained(this)),
      vector_icons::kCloseOldIcon);
}

void BraveProfileMenuView::BuildFeatureButtons() {
  Profile* profile = browser().GetProfile();
  if (profile && !profile->IsGuestSession() && !profile->IsOffTheRecord()) {
    if (auto* service =
            anar_account::AnarAccountServiceFactory::GetForProfile(profile)) {
      if (service->IsSignedIn()) {
        std::string label_id = service->GetNickname();
        if (label_id.empty()) {
          label_id = service->GetUsername();
        }
        const std::u16string manage_text =
            label_id.empty()
                ? u"Manage Anar Account"
                : base::UTF8ToUTF16("Anar Account (" + label_id + ")");
        AddFeatureButton(
            manage_text,
            base::BindRepeating(&BraveProfileMenuView::OnOpenAnarAccountClicked,
                                base::Unretained(this)),
            kAnarIconMonochromeIcon);
        AddFeatureButton(
            u"Sign out of Anar Account",
            base::BindRepeating(
                &BraveProfileMenuView::OnSignOutAnarAccountClicked,
                base::Unretained(this)),
            vector_icons::kSettingsIcon);
      } else {
        AddFeatureButton(
            u"Sign in to Anar Account",
            base::BindRepeating(&BraveProfileMenuView::OnOpenAnarAccountClicked,
                                base::Unretained(this)),
            kAnarIconMonochromeIcon);
      }
    }
  }
  MaybeBuildCloseBrowsersButton();
}

void BraveProfileMenuView::OnOpenAnarAccountClicked() {
  ShowSingletonTab(&browser(), GURL(kAnarAccountURL));
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

void BraveProfileMenuView::OnSignOutAnarAccountClicked() {
  if (auto* service = anar_account::AnarAccountServiceFactory::GetForProfile(
          browser().GetProfile())) {
    service->SignOut();
  }
  if (GetWidget()) {
    GetWidget()->Close();
  }
}
