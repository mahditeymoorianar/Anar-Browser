// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import { sendWithPromise } from 'chrome://resources/js/cr.js'
import { loadTimeData } from '../i18n_setup.js'

export interface VoiceSwapPersona {
  id: string
  name: string
  builtin: boolean
  hasIndex: boolean
}

export interface BravePrivacyBrowserProxy {
  wasPushMessagingEnabledAtStartup(): boolean
  wasWindowsRecallDisabledAtStartup(): boolean

  isWindowsRecallDisabled: () => Promise<boolean>
  setWindowsRecallDisabled: (disabled: boolean) => void

  getVoiceSwapPersonas: () => Promise<VoiceSwapPersona[]>
  addVoiceSwapPersona: (
    name: string, pthBase64: string, indexBase64: string
  ) => Promise<VoiceSwapPersona[]>
  removeVoiceSwapPersona: (id: string) => Promise<VoiceSwapPersona[]>
}

export class BravePrivacyBrowserProxyImpl implements BravePrivacyBrowserProxy {
  wasPushMessagingEnabledAtStartup(): boolean {
    return loadTimeData.getBoolean('pushMessagingEnabledAtStartup')
  }

  wasWindowsRecallDisabledAtStartup(): boolean {
    return loadTimeData.getBoolean('windowsRecallDisabledAtStartup')
  }

  isWindowsRecallDisabled(): Promise<boolean> {
    return sendWithPromise('isWindowsRecallDisabled')
  }

  setWindowsRecallDisabled(disabled: boolean) {
    chrome.send('setWindowsRecallDisabled', [disabled])
  }

  getVoiceSwapPersonas(): Promise<VoiceSwapPersona[]> {
    return sendWithPromise('getVoiceSwapPersonas')
  }

  addVoiceSwapPersona(
    name: string, pthBase64: string, indexBase64: string
  ): Promise<VoiceSwapPersona[]> {
    return sendWithPromise('addVoiceSwapPersona', name, pthBase64, indexBase64)
  }

  removeVoiceSwapPersona(id: string): Promise<VoiceSwapPersona[]> {
    return sendWithPromise('removeVoiceSwapPersona', id)
  }

  static getInstance(): BravePrivacyBrowserProxyImpl {
    return instance || (instance = new BravePrivacyBrowserProxyImpl())
  }
}

let instance: BravePrivacyBrowserProxy | null = null
