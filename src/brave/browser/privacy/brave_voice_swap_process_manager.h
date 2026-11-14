/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PRIVACY_BRAVE_VOICE_SWAP_PROCESS_MANAGER_H_
#define BRAVE_BROWSER_PRIVACY_BRAVE_VOICE_SWAP_PROCESS_MANAGER_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/process/process.h"

// Owns the lifetime of the local "Voice Swap" RVC server process — a
// separate Python process (not part of the browser's own source tree; see
// the .cc file's top comment for exactly what it runs and why) that the
// renderer's injected voice-swap script talks to over a loopback
// WebSocket. One instance for the whole browser (not per-profile): the
// server itself has no concept of profiles, so running more than one would
// just mean two processes fighting over the same port for no benefit.
//
// NOT thread-safe; UI-thread only, like the rest of //chrome/browser.
class BraveVoiceSwapProcessManager {
 public:
  static BraveVoiceSwapProcessManager* GetInstance();

  BraveVoiceSwapProcessManager(const BraveVoiceSwapProcessManager&) = delete;
  BraveVoiceSwapProcessManager& operator=(
      const BraveVoiceSwapProcessManager&) = delete;

  // Starts the server if it isn't already running (or already starting).
  // Safe to call redundantly — e.g. once per tab that turns the feature on.
  void EnsureRunning();

  // Stops the server if this manager started it. Not currently called
  // automatically when the feature is turned off for the last tab — the
  // idle server (no active streaming connections) costs nothing but a
  // little resident memory, and restarting it any time it's turned back on
  // would repay the ~10s model-load cost (see rvc_streaming.py) every time.
  void Stop();

  // True once the server has been launched *and* its auth token has been
  // read back from disk — i.e. actually usable, not merely "we called
  // LaunchProcess". Renderers should not be told voice_swap_enabled=true
  // in WebPreferences until this is true.
  bool IsReady() const { return !token_.empty(); }

  int port() const { return port_; }
  const std::string& token() const { return token_; }

  // Called whenever readiness changes (e.g. right after the token is read
  // back), so tab helpers waiting on it can re-push WebPreferences instead
  // of only doing so on their own unrelated triggers.
  using ReadyCallback = base::RepeatingClosure;
  base::CallbackListSubscription AddReadyCallback(ReadyCallback callback);

 private:
  friend base::NoDestructor<BraveVoiceSwapProcessManager>;
  BraveVoiceSwapProcessManager();
  ~BraveVoiceSwapProcessManager();

  void PollForToken(int attempts_remaining);
  void OnTokenRead(const std::string& token);

  base::Process process_;
  int port_ = 8021;
  std::string token_;
  bool launch_attempted_ = false;

  base::RepeatingClosureList ready_callbacks_;

  base::WeakPtrFactory<BraveVoiceSwapProcessManager> weak_factory_{this};
};

#endif  // BRAVE_BROWSER_PRIVACY_BRAVE_VOICE_SWAP_PROCESS_MANAGER_H_
