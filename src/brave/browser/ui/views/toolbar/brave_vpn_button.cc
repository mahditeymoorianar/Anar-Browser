/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/toolbar/brave_vpn_button.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "brave/browser/ui/color/brave_color_id.h"
#include "brave/components/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/grit/brave_components_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

// Verified Chrome Web Store id for the official Proton VPN extension
// (publisher: Proton AG, "Proton VPN: Fast & Secure").
constexpr char kProtonVpnExtensionId[] = "jplgfhpmjnbigmhklmmbgecoobifkmpa";

enum class VpnProviderCommand {
  kOpenProtonVpn = 1,
  kOpenMullvadVpn = 2,
};

constexpr SkColor kProtonVpnIconColor = SkColorSetRGB(0x6D, 0x4A, 0xFF);
constexpr SkColor kMullvadVpnIconColor = SkColorSetRGB(0x29, 0x4D, 0x73);

void OpenUrlInNewTab(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

ui::ImageModel GetMullvadIcon() {
  const gfx::Image& image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_ANAR_MULLVAD_ICON_32);
  if (image.IsEmpty()) {
    return ui::ImageModel::FromVectorIcon(kLeoProductVpnIcon,
                                          kMullvadVpnIconColor, 16);
  }
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateResizedImage(
          image.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
          gfx::Size(16, 16)));
}

ToolbarActionViewModel* GetExtensionAction(Browser* browser,
                                           const std::string& extension_id) {
  auto* container = ExtensionsContainer::From(*browser);
  return container ? container->GetActionForId(extension_id) : nullptr;
}

ui::ImageModel GetExtensionIcon(Browser* browser,
                                const std::string& extension_id) {
  auto* action = GetExtensionAction(browser, extension_id);
  if (!action) {
    return ui::ImageModel();
  }
  return action->GetIcon(browser->tab_strip_model()->GetActiveWebContents(),
                         gfx::Size(16, 16));
}

class VpnProviderPickerModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate {
 public:
  explicit VpnProviderPickerModel(Browser* browser)
      : SimpleMenuModel(this), browser_(browser) {
    ui::ImageModel proton_icon =
        GetExtensionIcon(browser, kProtonVpnExtensionId);
    if (proton_icon.IsEmpty()) {
      proton_icon = ui::ImageModel::FromVectorIcon(kLeoProductVpnIcon,
                                                   kProtonVpnIconColor, 16);
    }
    AddItemWithIcon(static_cast<int>(VpnProviderCommand::kOpenProtonVpn),
                    u"Proton VPN", proton_icon);

    AddItemWithIcon(static_cast<int>(VpnProviderCommand::kOpenMullvadVpn),
                    u"Mullvad VPN", GetMullvadIcon());
  }

  ~VpnProviderPickerModel() override = default;
  VpnProviderPickerModel(const VpnProviderPickerModel&) = delete;
  VpnProviderPickerModel& operator=(const VpnProviderPickerModel&) = delete;

 private:
  void ExecuteCommand(int command_id, int event_flags) override {
    switch (static_cast<VpnProviderCommand>(command_id)) {
      case VpnProviderCommand::kOpenProtonVpn: {
        if (auto* action =
                GetExtensionAction(browser_, kProtonVpnExtensionId)) {
          action->ExecuteUserAction(
              ToolbarActionViewModel::InvocationSource::kMenuEntry);
          return;
        }
        OpenUrlInNewTab(
            browser_,
            GURL("https://chromewebstore.google.com/detail/"
                 "proton-vpn-fast-secure/" +
                 std::string(kProtonVpnExtensionId)));
        return;
      }
      case VpnProviderCommand::kOpenMullvadVpn:
        OpenUrlInNewTab(browser_, GURL("https://mullvad.net/en/download"));
        return;
    }
  }

  raw_ptr<Browser, DanglingUntriaged> browser_ = nullptr;
};

}  // namespace

BraveVPNButton::BraveVPNButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&BraveVPNButton::OnButtonPressed,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr,
                    false),
      browser_(browser) {
  auto menu_button_controller = std::make_unique<views::MenuButtonController>(
      this,
      base::BindRepeating(&BraveVPNButton::OnButtonPressed,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
  SetAccessibleName(u"VPN");

  // Warm the Proton extension's icon now so the first menu open shows it.
  std::ignore = GetExtensionIcon(browser_, kProtonVpnExtensionId);
}

BraveVPNButton::~BraveVPNButton() = default;

void BraveVPNButton::UpdateColorsAndInsets() {
  ui::ColorProvider* cp = GetColorProvider();
  if (!cp) {
    return;
  }

  const int radius = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, {});
  SetBackground(views::CreateRoundedRectBackground(
      cp->GetColor(kColorToolbar), radius));

  const int button_size =
      GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  const int icon_size = GetIconSize();
  const int pad_x = std::max(0, (button_size - icon_size) / 2);
  const int pad_y = std::max(0, (button_size - icon_size) / 2);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(pad_y, pad_x)));

  const gfx::ImageSkia vpn_image =
      gfx::CreateVectorIcon(kLeoProductVpnIcon, icon_size, GetIconColor());
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromImageSkia(vpn_image));
  SetImageModel(views::Button::STATE_DISABLED,
                ui::ImageModel::FromImageSkia(
                    gfx::ImageSkiaOperations::CreateTransparentImage(
                        vpn_image, gfx::kDisabledControlAlpha / 255.0)));
}

SkColor BraveVPNButton::GetIconColor() {
  ui::ColorProvider* cp = GetColorProvider();
  CHECK(cp);

  const auto ink_drop_state =
      views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState();
  ui::ColorId icon_color_id = kColorToolbarButtonIcon;
  if (ink_drop_state == views::InkDropState::ACTIVATED) {
    icon_color_id = kColorToolbarButtonActivated;
  }

  return cp->GetColor(icon_color_id);
}

std::u16string BraveVPNButton::GetRenderedTooltipText(
    const gfx::Point& p) const {
  return u"VPN";
}

void BraveVPNButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  views::InkDrop::Get(this)->SetVisibleOpacity(0.00f);
  views::InkDrop::Get(this)->SetHighlightOpacity(1.0f);
  UpdateColorsAndInsets();
}

void BraveVPNButton::InkDropRippleAnimationEnded(views::InkDropState state) {
  UpdateColorsAndInsets();
}

void BraveVPNButton::OnButtonPressed(const ui::Event& event) {
  ShowVpnProviderMenu(event);
}

void BraveVPNButton::ShowVpnProviderMenu(const ui::Event& event) {
  vpn_provider_menu_model_ =
      std::make_unique<VpnProviderPickerModel>(browser_);
  vpn_provider_menu_runner_ = std::make_unique<views::MenuRunner>(
      vpn_provider_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  vpn_provider_menu_runner_->RunMenuAt(
      GetWidget(), menu_button_controller_, GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

BEGIN_METADATA(BraveVPNButton)
END_METADATA
