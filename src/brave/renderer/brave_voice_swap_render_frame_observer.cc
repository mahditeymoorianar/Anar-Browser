/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/renderer/brave_voice_swap_render_frame_observer.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace {

// Kept deliberately simple (ScriptProcessorNode, linear resampling) rather
// than an AudioWorklet + polyphase resampler: this is a single script
// injected as one blob with no separate module file to load, and the
// server-side RVC inference (see rvc_streaming.py) is the real bottleneck
// by a wide margin — sub-millisecond resampling quality differences are not
// what's limiting this feature's latency or fidelity.
constexpr char kVoiceSwapScriptTemplate[] = R"JS((function() {
  const config = %s;
  if (!config.enabled || typeof navigator === 'undefined' ||
      !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    return;
  }

  function resampleLinear(input, inRate, outRate) {
    if (inRate === outRate) return input;
    const ratio = inRate / outRate;
    const outLength = Math.max(1, Math.round(input.length / ratio));
    const out = new Float32Array(outLength);
    for (let i = 0; i < outLength; i++) {
      const srcIndex = i * ratio;
      const i0 = Math.floor(srcIndex);
      const i1 = Math.min(i0 + 1, input.length - 1);
      const frac = srcIndex - i0;
      out[i] = input[i0] * (1 - frac) + input[i1] * frac;
    }
    return out;
  }

  async function swapAudio(originalStream) {
    const audioTrack = originalStream.getAudioTracks()[0];
    if (!audioTrack) return originalStream;

    const audioCtx = new AudioContext();
    const source = audioCtx.createMediaStreamSource(
        new MediaStream([audioTrack]));
    const dest = audioCtx.createMediaStreamDestination();

    const wsUrl = 'ws://127.0.0.1:' + config.port + '/stream?token=' +
        encodeURIComponent(config.token) + '&persona_id=' +
        encodeURIComponent(config.personaId);
    const ws = new WebSocket(wsUrl);
    ws.binaryType = 'arraybuffer';

    const TARGET_RATE = 16000;
    const inRate = audioCtx.sampleRate;
    let outQueue = [];

    ws.onmessage = (ev) => {
      if (typeof ev.data === 'string') return;  // the initial {"type":"ready",...}
      outQueue.push(new Float32Array(ev.data));
    };
    ws.onerror = () => {
      console.warn('[Anar Voice Swap] server connection failed; ' +
          'your real voice will be sent instead.');
    };

    // Pulls mic audio out of the graph and forwards it to the server.
    const inputTap = audioCtx.createScriptProcessor(4096, 1, 1);
    source.connect(inputTap);
    const inputSink = audioCtx.createGain();
    inputSink.gain.value = 0;
    inputTap.connect(inputSink);
    inputSink.connect(audioCtx.destination);
    inputTap.onaudioprocess = (e) => {
      if (ws.readyState !== WebSocket.OPEN) return;
      const resampled = resampleLinear(
          e.inputBuffer.getChannelData(0), inRate, TARGET_RATE);
      ws.send(resampled.buffer);
    };

    // Pulls converted audio out of outQueue and into the outgoing stream.
    const outputTap = audioCtx.createScriptProcessor(4096, 1, 1);
    const driver = audioCtx.createConstantSource();
    driver.connect(outputTap);
    driver.start();
    let pending = new Float32Array(0);
    outputTap.onaudioprocess = (e) => {
      const output = e.outputBuffer.getChannelData(0);
      while (pending.length < output.length && outQueue.length) {
        const next = outQueue.shift();
        const merged = new Float32Array(pending.length + next.length);
        merged.set(pending);
        merged.set(next, pending.length);
        pending = merged;
      }
      if (pending.length >= output.length) {
        output.set(pending.subarray(0, output.length));
        pending = pending.subarray(output.length);
      } else {
        output.fill(0);  // Not enough converted audio yet — emit silence
                          // rather than stale/garbage data.
      }
    };
    outputTap.connect(dest);

    const outStream = dest.stream;
    originalStream.getVideoTracks().forEach((t) => outStream.addTrack(t));
    return outStream;
  }

  const realGetUserMedia =
      navigator.mediaDevices.getUserMedia.bind(navigator.mediaDevices);
  navigator.mediaDevices.getUserMedia = async function(constraints) {
    const stream = await realGetUserMedia(constraints);
    if (!constraints || !constraints.audio) {
      return stream;
    }
    try {
      return await swapAudio(stream);
    } catch (e) {
      console.warn('[Anar Voice Swap] failed, using your real voice:', e);
      return stream;
    }
  };
})();)JS";

std::string BuildVoiceSwapScript(const blink::web_pref::WebPreferences& prefs) {
  base::DictValue config;
  config.Set("enabled", prefs.voice_swap_enabled);
  config.Set("personaId", prefs.voice_swap_persona_id);
  config.Set("port", prefs.voice_swap_port);
  config.Set("token", prefs.voice_swap_token);

  std::string config_json;
  base::JSONWriter::Write(config, &config_json);
  return base::StringPrintf(kVoiceSwapScriptTemplate, config_json.c_str());
}

}  // namespace

BraveVoiceSwapRenderFrameObserver::BraveVoiceSwapRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

BraveVoiceSwapRenderFrameObserver::~BraveVoiceSwapRenderFrameObserver() =
    default;

void BraveVoiceSwapRenderFrameObserver::DidClearWindowObject() {
  if (!render_frame() || !render_frame()->GetBlinkPreferences().voice_swap_enabled) {
    return;
  }
  std::string script = BuildVoiceSwapScript(render_frame()->GetBlinkPreferences());
  render_frame()->GetWebFrame()->ExecuteScript(
      blink::WebScriptSource(blink::WebString::FromUtf8(script)));
}

void BraveVoiceSwapRenderFrameObserver::OnDestruct() {
  delete this;
}
