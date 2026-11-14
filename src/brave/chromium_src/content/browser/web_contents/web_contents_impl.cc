/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "content/browser/web_contents/web_contents_impl.h"

#include <utility>

#include "base/time/time.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace content {

bool WebContentsImpl::GetShouldDoLearningForTesting() {
  return ShouldDoLearning();
}

void WebContentsImpl::SetFakeGeopositionOverride(double latitude,
                                                  double longitude) {
  auto position = device::mojom::Geoposition::New();
  position->latitude = latitude;
  position->longitude = longitude;
  position->accuracy = 100;
  position->timestamp = base::Time::Now();
  GetGeolocationContext()->SetOverride(
      device::mojom::GeopositionResult::NewPosition(std::move(position)));
}

void WebContentsImpl::ClearFakeGeopositionOverride() {
  GetGeolocationContext()->ClearOverride();
}

void WebContentsImpl::BraveSetDeviceEmulationSize(const gfx::Size& size) {
  SetDeviceEmulationSize(size);
}

void WebContentsImpl::BraveClearDeviceEmulationSize() {
  ClearDeviceEmulationSize();
}

}  // namespace content

#include <content/browser/web_contents/web_contents_impl.cc>
