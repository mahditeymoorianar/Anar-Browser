/* Copyright (c) 2026 The Anar Browser Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_WEBUI_ANAR_ACCOUNT_ANAR_ACCOUNT_UI_H_
#define BRAVE_BROWSER_UI_WEBUI_ANAR_ACCOUNT_ANAR_ACCOUNT_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

class AnarAccountUI : public content::WebUIController {
 public:
  explicit AnarAccountUI(content::WebUI* web_ui);
  AnarAccountUI(const AnarAccountUI&) = delete;
  AnarAccountUI& operator=(const AnarAccountUI&) = delete;
  ~AnarAccountUI() override;
};

#endif  // BRAVE_BROWSER_UI_WEBUI_ANAR_ACCOUNT_ANAR_ACCOUNT_UI_H_
