/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_RENDERER_BRAVE_VOICE_SWAP_RENDER_FRAME_OBSERVER_H_
#define BRAVE_RENDERER_BRAVE_VOICE_SWAP_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

// When the "Voice Swap" privacy feature is on for this tab (per
// WebPreferences — see the .mojom field's comment), installs a main-world
// script at the earliest possible point in every new document
// (DidClearWindowObject, before any page script runs) that transparently
// wraps navigator.mediaDevices.getUserMedia(): audio-requesting calls get
// their returned track's audio relayed through the local voice-swap server
// and replaced with the converted result, so *every* site the user talks to
// — a video call, a recorder, anything using the mic — hears the chosen
// persona's voice, with zero per-site cooperation needed.
class BraveVoiceSwapRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit BraveVoiceSwapRenderFrameObserver(content::RenderFrame* render_frame);
  BraveVoiceSwapRenderFrameObserver(const BraveVoiceSwapRenderFrameObserver&) =
      delete;
  BraveVoiceSwapRenderFrameObserver& operator=(
      const BraveVoiceSwapRenderFrameObserver&) = delete;
  ~BraveVoiceSwapRenderFrameObserver() override;

  // content::RenderFrameObserver:
  void DidClearWindowObject() override;
  void OnDestruct() override;
};

#endif  // BRAVE_RENDERER_BRAVE_VOICE_SWAP_RENDER_FRAME_OBSERVER_H_
