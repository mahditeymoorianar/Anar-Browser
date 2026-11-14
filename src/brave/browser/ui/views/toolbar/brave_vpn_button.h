/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_TOOLBAR_BRAVE_VPN_BUTTON_H_
#define BRAVE_BROWSER_UI_VIEWS_TOOLBAR_BRAVE_VPN_BUTTON_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button_controller.h"

namespace views {
class MenuRunner;
}  // namespace views

namespace ui {
class SimpleMenuModel;
}  // namespace ui

class Browser;

// Toolbar VPN provider picker button (Proton VPN and Mullvad VPN).
class BraveVPNButton : public ToolbarButton {
  METADATA_HEADER(BraveVPNButton, ToolbarButton)
 public:
  explicit BraveVPNButton(Browser* browser);
  ~BraveVPNButton() override;

  BraveVPNButton(const BraveVPNButton&) = delete;
  BraveVPNButton& operator=(const BraveVPNButton&) = delete;

 private:
  // ToolbarButton overrides:
  void UpdateColorsAndInsets() override;
  std::u16string GetRenderedTooltipText(const gfx::Point& p) const override;
  void OnThemeChanged() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  void OnButtonPressed(const ui::Event& event);
  void ShowVpnProviderMenu(const ui::Event& event);
  SkColor GetIconColor();

  raw_ptr<Browser, DanglingUntriaged> browser_ = nullptr;
  raw_ptr<views::MenuButtonController> menu_button_controller_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> vpn_provider_menu_model_;
  std::unique_ptr<views::MenuRunner> vpn_provider_menu_runner_;
};

#endif  // BRAVE_BROWSER_UI_VIEWS_TOOLBAR_BRAVE_VPN_BUTTON_H_
