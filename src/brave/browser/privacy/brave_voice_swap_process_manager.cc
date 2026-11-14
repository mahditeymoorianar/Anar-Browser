/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/privacy/brave_voice_swap_process_manager.h"

#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace {

// The "Voice Swap" RVC engine lives in a sibling project to the browser's
// own source tree (see rvc-test/README.md), not inside src/brave — it's a
// separate Python/FastAPI service the browser launches on demand, the same
// way it would any other local helper process. ANAR_VOICE_SWAP_BACKEND_DIR
// overrides this for anyone whose checkout of that project lives somewhere
// else; the hardcoded default matches this machine's actual layout.
base::FilePath GetBackendDir() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::optional<std::string> override_dir =
      env->GetVar("ANAR_VOICE_SWAP_BACKEND_DIR");
  if (override_dir.has_value() && !override_dir->empty()) {
    return base::FilePath::FromUTF8Unsafe(*override_dir);
  }
  return base::FilePath(FILE_PATH_LITERAL(
      "C:\\projects\\voicer\\rvc-test\\backend"));
}

std::optional<std::string> ReadTokenFileBlocking(base::FilePath backend_dir) {
  base::FilePath token_path =
      backend_dir.DirName().Append(FILE_PATH_LITERAL(".voice_swap_token"));
  std::string contents;
  if (!base::ReadFileToString(token_path, &contents)) {
    return std::nullopt;
  }
  contents = base::TrimWhitespaceASCII(contents, base::TRIM_ALL).data();
  if (contents.empty()) {
    return std::nullopt;
  }
  return contents;
}

}  // namespace

// static
BraveVoiceSwapProcessManager* BraveVoiceSwapProcessManager::GetInstance() {
  static base::NoDestructor<BraveVoiceSwapProcessManager> instance;
  return instance.get();
}

BraveVoiceSwapProcessManager::BraveVoiceSwapProcessManager() = default;

BraveVoiceSwapProcessManager::~BraveVoiceSwapProcessManager() {
  Stop();
}

void BraveVoiceSwapProcessManager::EnsureRunning() {
  if (launch_attempted_) {
    return;
  }
  launch_attempted_ = true;

  base::FilePath backend_dir = GetBackendDir();

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python")));
  cmd.AppendArg("-m");
  cmd.AppendArg("app.voice_swap_server");

  base::LaunchOptions options;
  options.current_directory = backend_dir;
  // Inherit the parent's environment (needs the same Python + installed
  // packages the browser's own environment already has, per the manual
  // dependency check done when this feature was built) rather than
  // starting from an empty one.
  options.environment = {};

  process_ = base::LaunchProcess(cmd, options);
  if (!process_.IsValid()) {
    LOG(ERROR) << "Voice Swap: failed to launch " << backend_dir;
    return;
  }

  // The server writes its auth token to disk within the first moment of
  // starting (well before uvicorn is actually accepting connections), so
  // poll for that file rather than trying to detect "the port is open".
  PollForToken(/*attempts_remaining=*/40);  // ~20s at 500ms apart
}

void BraveVoiceSwapProcessManager::PollForToken(int attempts_remaining) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadTokenFileBlocking, GetBackendDir()),
      base::BindOnce(
          [](base::WeakPtr<BraveVoiceSwapProcessManager> self,
             int attempts_remaining, std::optional<std::string> token) {
            if (!self) {
              return;
            }
            if (token.has_value()) {
              self->OnTokenRead(*token);
              return;
            }
            if (attempts_remaining <= 0) {
              LOG(ERROR) << "Voice Swap: server did not report an auth "
                            "token in time; giving up.";
              return;
            }
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(&BraveVoiceSwapProcessManager::PollForToken,
                                self, attempts_remaining - 1),
                base::Milliseconds(500));
          },
          weak_factory_.GetWeakPtr(), attempts_remaining));
}

void BraveVoiceSwapProcessManager::OnTokenRead(const std::string& token) {
  token_ = token;
  ready_callbacks_.Notify();
}

void BraveVoiceSwapProcessManager::Stop() {
  if (process_.IsValid()) {
    process_.Terminate(/*exit_code=*/0, /*wait=*/false);
  }
  process_ = base::Process();
  token_.clear();
  launch_attempted_ = false;
}

base::CallbackListSubscription BraveVoiceSwapProcessManager::AddReadyCallback(
    ReadyCallback callback) {
  return ready_callbacks_.Add(std::move(callback));
}
