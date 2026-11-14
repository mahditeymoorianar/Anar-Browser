// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import { PolymerElement } from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'

import {
  WebUiListenerMixin,
  WebUiListenerMixinInterface,
} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js'
import {
  PrefsMixin,
  PrefsMixinInterface,
} from '/shared/settings/prefs/prefs_mixin.js'
import { loadTimeData } from '../i18n_setup.js'

import { getTemplate } from './brave_personalization_options.html.js'
import {
  BravePrivacyBrowserProxy,
  BravePrivacyBrowserProxyImpl,
  VoiceSwapPersona,
} from './brave_privacy_page_browser_proxy.js'

import { assert } from 'chrome://resources/js/assert.js'
import { SettingsToggleButtonElement } from '../controls/settings_toggle_button.js'

import '../controls/settings_dropdown_menu.js'
import '../privacy_page/do_not_track_toggle.js'
import 'chrome://resources/cr_elements/cr_input/cr_input.js'
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js'

// Curated fallback used only if the JS engine doesn't support
// Intl.supportedValuesOf('timeZone') (it does in every Chromium version this
// browser ships, but this keeps the dropdown non-empty if that ever regresses
// rather than silently breaking the fake-timezone feature).
const FALLBACK_TIMEZONE_IDS_: string[] = [
  'Pacific/Honolulu', 'America/Anchorage', 'America/Los_Angeles',
  'America/Denver', 'America/Chicago', 'America/New_York',
  'America/Sao_Paulo', 'UTC', 'Europe/London', 'Europe/Paris',
  'Europe/Athens', 'Europe/Moscow', 'Asia/Tehran', 'Asia/Dubai',
  'Asia/Karachi', 'Asia/Kolkata', 'Asia/Dhaka', 'Asia/Bangkok',
  'Asia/Shanghai', 'Asia/Tokyo', 'Australia/Sydney', 'Pacific/Auckland',
]

// Formats a UTC offset like "+03:30" for a given IANA timezone id, as of
// today's date (offsets can shift with DST, but this is only used to label
// a dropdown entry, not for the actual spoofed value).
function formatUtcOffset_(timeZoneId: string): string {
  try {
    const parts = new Intl.DateTimeFormat('en-US', {
      timeZone: timeZoneId,
      timeZoneName: 'shortOffset',
    }).formatToParts(new Date())
    const offsetPart = parts.find((p) => p.type === 'timeZoneName')
    return offsetPart ? offsetPart.value.replace('GMT', 'UTC') : ''
  } catch (e) {
    return ''
  }
}

// Builds the full list of selectable fake-timezone options: every IANA
// timezone the platform knows about, labeled with its current UTC offset.
function getAllTimezoneOptions_(): Array<{ value: string, name: string }> {
  let ids: string[] = FALLBACK_TIMEZONE_IDS_
  const supportedValuesOf = (Intl as unknown as {
    supportedValuesOf?: (key: string) => string[]
  }).supportedValuesOf
  if (typeof supportedValuesOf === 'function') {
    try {
      ids = supportedValuesOf('timeZone')
    } catch (e) {
      // Keep the fallback list.
    }
  }
  return ids.map((id) => {
    const offset = formatUtcOffset_(id)
    const label = id.replace(/_/g, ' ')
    return { value: id, name: offset ? `${label} (${offset})` : label }
  })
}

const SettingsBravePersonalizationOptionsBase = PrefsMixin(
  WebUiListenerMixin(PolymerElement),
) as {
  new (): PolymerElement & WebUiListenerMixinInterface & PrefsMixinInterface
}

export class SettingsBravePersonalizationOptions extends SettingsBravePersonalizationOptionsBase {
  static get is() {
    return 'settings-brave-personalization-options'
  }

  static get template() {
    return getTemplate()
  }

  static get properties() {
    return {
      webRTCPolicies_: {
        readOnly: true,
        type: Array,
        value: function () {
          return [
            { value: 'default', name: loadTimeData.getString('webRTCDefault') },
            {
              value: 'default_public_and_private_interfaces',
              name: loadTimeData.getString('defaultPublicAndPrivateInterfaces'),
            },
            {
              value: 'default_public_interface_only',
              name: loadTimeData.getString('defaultPublicInterfaceOnly'),
            },
            {
              value: 'disable_non_proxied_udp',
              name: loadTimeData.getString('disableNonProxiedUdp'),
            },
          ]
        },
      },
      webRTCPolicy_: String,
      // Preset places offered for the fake location, plus a "Custom
      // coordinates…" escape hatch. Coordinates are city-centre points; the
      // exact values matter less than being plausibly inside the city, since
      // the accuracy reported to sites is deliberately coarse.
      fakeLocationPlaces_: {
        readOnly: true,
        type: Array,
        value: function () {
          return [
            { value: 'custom', name: loadTimeData.getString('fakeLocationCustom') },
            { value: '35.6762,139.6503', name: 'Tokyo, Japan' },
            { value: '28.6139,77.2090', name: 'Delhi, India' },
            { value: '31.2304,121.4737', name: 'Shanghai, China' },
            { value: '-23.5505,-46.6333', name: 'São Paulo, Brazil' },
            { value: '19.4326,-99.1332', name: 'Mexico City, Mexico' },
            { value: '30.0444,31.2357', name: 'Cairo, Egypt' },
            { value: '19.0760,72.8777', name: 'Mumbai, India' },
            { value: '35.6892,51.3890', name: 'Tehran, Iran' },
            { value: '41.0082,28.9784', name: 'Istanbul, Türkiye' },
            { value: '55.7558,37.6173', name: 'Moscow, Russia' },
            { value: '51.5074,-0.1278', name: 'London, United Kingdom' },
            { value: '48.8566,2.3522', name: 'Paris, France' },
            { value: '52.5200,13.4050', name: 'Berlin, Germany' },
            { value: '40.7128,-74.0060', name: 'New York, United States' },
            { value: '34.0522,-118.2437', name: 'Los Angeles, United States' },
            { value: '43.6532,-79.3832', name: 'Toronto, Canada' },
            { value: '-33.8688,151.2093', name: 'Sydney, Australia' },
            { value: '-26.2041,28.0473', name: 'Johannesburg, South Africa' },
            { value: '1.3521,103.8198', name: 'Singapore' },
            { value: '25.2048,55.2708', name: 'Dubai, UAE' },
            { value: '37.5665,126.9780', name: 'Seoul, South Korea' },
            { value: '39.9042,116.4074', name: 'Beijing, China' },
            { value: '13.7563,100.5018', name: 'Bangkok, Thailand' },
            { value: '3.1390,101.6869', name: 'Kuala Lumpur, Malaysia' },
            { value: '14.5995,120.9842', name: 'Manila, Philippines' },
            { value: '-6.2088,106.8456', name: 'Jakarta, Indonesia' },
            { value: '23.8103,90.4125', name: 'Dhaka, Bangladesh' },
            { value: '24.8607,67.0011', name: 'Karachi, Pakistan' },
            { value: '33.6844,73.0479', name: 'Islamabad, Pakistan' },
            { value: '34.6937,135.5023', name: 'Osaka, Japan' },
            { value: '22.3193,114.1694', name: 'Hong Kong' },
            { value: '25.0330,121.5654', name: 'Taipei, Taiwan' },
            { value: '21.0285,105.8542', name: 'Hanoi, Vietnam' },
            { value: '31.7683,35.2137', name: 'Jerusalem, Israel' },
            { value: '33.8938,35.5018', name: 'Beirut, Lebanon' },
            { value: '24.7136,46.6753', name: 'Riyadh, Saudi Arabia' },
            { value: '29.3759,47.9774', name: 'Kuwait City, Kuwait' },
            { value: '25.2854,51.5310', name: 'Doha, Qatar' },
            { value: '35.6892,51.3890', name: 'Tehran, Iran' },
            { value: '50.4501,30.5234', name: 'Kyiv, Ukraine' },
            { value: '52.2297,21.0122', name: 'Warsaw, Poland' },
            { value: '50.0755,14.4378', name: 'Prague, Czechia' },
            { value: '48.2082,16.3738', name: 'Vienna, Austria' },
            { value: '47.4979,19.0402', name: 'Budapest, Hungary' },
            { value: '59.3293,18.0686', name: 'Stockholm, Sweden' },
            { value: '59.9139,10.7522', name: 'Oslo, Norway' },
            { value: '55.6761,12.5683', name: 'Copenhagen, Denmark' },
            { value: '52.3676,4.9041', name: 'Amsterdam, Netherlands' },
            { value: '50.8503,4.3517', name: 'Brussels, Belgium' },
            { value: '46.9480,7.4474', name: 'Bern, Switzerland' },
            { value: '41.9028,12.4964', name: 'Rome, Italy' },
            { value: '40.4168,-3.7038', name: 'Madrid, Spain' },
            { value: '38.7223,-9.1393', name: 'Lisbon, Portugal' },
            { value: '37.9838,23.7275', name: 'Athens, Greece' },
            { value: '44.4268,26.1025', name: 'Bucharest, Romania' },
            { value: '53.3498,-6.2603', name: 'Dublin, Ireland' },
            { value: '59.4370,24.7536', name: 'Tallinn, Estonia' },
            { value: '-34.6037,-58.3816', name: 'Buenos Aires, Argentina' },
            { value: '-33.4489,-70.6693', name: 'Santiago, Chile' },
            { value: '4.7110,-74.0721', name: 'Bogotá, Colombia' },
            { value: '10.4806,-66.9036', name: 'Caracas, Venezuela' },
            { value: '-12.0464,-77.0428', name: 'Lima, Peru' },
            { value: '45.4215,-75.6972', name: 'Ottawa, Canada' },
            { value: '49.2827,-123.1207', name: 'Vancouver, Canada' },
            { value: '37.7749,-122.4194', name: 'San Francisco, United States' },
            { value: '41.8781,-87.6298', name: 'Chicago, United States' },
            { value: '38.9072,-77.0369', name: 'Washington, United States' },
            { value: '25.7617,-80.1918', name: 'Miami, United States' },
            { value: '-1.2921,36.8219', name: 'Nairobi, Kenya' },
            { value: '6.5244,3.3792', name: 'Lagos, Nigeria' },
            { value: '30.0444,31.2357', name: 'Cairo, Egypt' },
            { value: '33.5731,-7.5898', name: 'Casablanca, Morocco' },
            { value: '36.8065,10.1815', name: 'Tunis, Tunisia' },
            { value: '-33.9249,18.4241', name: 'Cape Town, South Africa' },
            { value: '-37.8136,144.9631', name: 'Melbourne, Australia' },
            { value: '-36.8485,174.7633', name: 'Auckland, New Zealand' },
          ]
        },
      },
      // The currently selected entry of |fakeLocationPlaces_|: either
      // "<lat>,<lng>" matching a preset, or 'custom'.
      selectedPlace_: {
        type: String,
        value: 'custom',
      },
      latitudeInput_: String,
      longitudeInput_: String,
      latitudeInvalid_: {
        type: Boolean,
        value: false,
      },
      longitudeInvalid_: {
        type: Boolean,
        value: false,
      },
      // Kept in sync with the presets defined in
      // brave/browser/privacy/brave_fake_device_info_presets.cc: the
      // |value| here must match a preset's |id| there.
      fakeDeviceInfoPresets_: {
        readOnly: true,
        type: Array,
        value: function () {
          return [
            { value: 'windows', name: 'Windows desktop' },
            { value: 'macos', name: 'macOS desktop' },
            { value: 'linux', name: 'Linux desktop' },
            { value: 'android', name: 'Android phone' },
          ]
        },
      },
      voiceSwapPersonaOptions_: {
        type: Array,
        value: () => [],
      },
      showAddPersonaForm_: {
        type: Boolean,
        value: false,
      },
      newPersonaName_: String,
      addPersonaError_: {
        type: Boolean,
        value: false,
      },
      canSavePersona_: {
        type: Boolean,
        value: false,
      },
      selectedPersonaIsCustom_: {
        type: Boolean,
        value: false,
      },
      fakeTimezoneOptions_: {
        readOnly: true,
        type: Array,
        value: function () {
          return getAllTimezoneOptions_()
        },
      },
      historyRetentionOptions_: {
        readOnly: true,
        type: Array,
        value: function () {
          return [
            { value: 1, name: loadTimeData.getString('historyRetentionOneDay') },
            {
              value: 7,
              name: loadTimeData.getString('historyRetentionOneWeek'),
            },
            {
              value: 30,
              name: loadTimeData.getString('historyRetentionOneMonth'),
            },
            {
              value: 90,
              name: loadTimeData.getString('historyRetentionThreeMonths'),
            },
            {
              value: 180,
              name: loadTimeData.getString('historyRetentionSixMonths'),
            },
            {
              value: 365,
              name: loadTimeData.getString('historyRetentionOneYear'),
            },
            {
              value: 365 * 5,
              name: loadTimeData.getString('historyRetentionFiveYears'),
            },
            {
              value: -1,
              name: loadTimeData.getString('historyRetentionForever'),
            },
          ]
        },
      },
      isDebounceFeatureEnabled_: {
        readOnly: true,
        type: Boolean,
        value: function () {
          return loadTimeData.getBoolean('isDebounceFeatureEnabled')
        },
      },
      isRequestOTRFeatureEnabled_: {
        readOnly: true,
        type: Boolean,
        value: function () {
          return loadTimeData.getBoolean('isRequestOTRFeatureEnabled')
        },
      },
      isPsstFeatureEnabled_: {
        readOnly: true,
        type: Boolean,
        value: function () {
          return loadTimeData.getBoolean('isPsstFeatureEnabled')
        },
      },
      requestOTRActions_: {
        readOnly: true,
        type: Array,
        value: function () {
          return [
            { value: 0, name: loadTimeData.getString('requestOTRDefault') },
            { value: 1, name: loadTimeData.getString('requestOTRAlways') },
            { value: 2, name: loadTimeData.getString('requestOTRNever') },
          ]
        },
      },
      requestOTRAction_: String,
      isWindowsRecallAvailable_: {
        readOnly: true,
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isWindowsRecallAvailable')
        },
      },
      windowsRecallDisabledPref_: {
        type: Object,
        value: () => {
          return {}
        },
      },
    }
  }

  static get observers() {
    return [
      'onFakeLocationCoordsChanged_(' +
        'prefs.brave.privacy.fake_geolocation_latitude.value, ' +
        'prefs.brave.privacy.fake_geolocation_longitude.value)',
      'onVoiceSwapSelectionChanged_(' +
        'voiceSwapPersonaOptions_, prefs.brave.privacy.voice_swap_persona_id.value)',
      'updateCanSavePersona_(newPersonaName_)',
    ]
  }

  declare private webRTCPolicies_: Object[]
  declare private webRTCPolicy_: String
  declare private fakeLocationPlaces_: Object[]
  declare private selectedPlace_: string
  declare private latitudeInput_: string
  declare private longitudeInput_: string
  declare private latitudeInvalid_: boolean
  declare private longitudeInvalid_: boolean
  declare private voiceSwapPersonaOptions_: Object[]
  declare private showAddPersonaForm_: boolean
  declare private newPersonaName_: string
  declare private addPersonaError_: boolean
  declare private canSavePersona_: boolean
  declare private selectedPersonaIsCustom_: boolean
  declare private fakeDeviceInfoPresets_: Object[]
  declare private fakeTimezoneOptions_: Object[]
  declare private historyRetentionOptions_: Object[]

  // Whether |selectedPlace_| has been seeded from the stored coordinates yet.
  private placeInitialized_: boolean = false

  // Raw file picks for the add-persona form. Not Polymer properties: only
  // whether they're present (canSavePersona_) needs to be reactive, not the
  // File objects themselves.
  private newPersonaModelFile_: File | null = null
  private newPersonaIndexFile_: File | null = null
  private personasById_: Map<string, VoiceSwapPersona> = new Map()
  declare private isDebounceFeatureEnabled_: boolean
  declare private isRequestOTRFeatureEnabled_: boolean
  declare private isPsstFeatureEnabled_: boolean
  declare private requestOTRActions_: Object[]
  declare private requestOTRAction_: String
  declare private isWindowsRecallAvailable_: boolean
  declare private windowsRecallDisabledPref_: chrome.settingsPrivate.PrefObject

  browserProxy_: BravePrivacyBrowserProxy =
    BravePrivacyBrowserProxyImpl.getInstance()

  shouldShowRestart_(enabled: boolean) {
    return enabled != this.browserProxy_.wasPushMessagingEnabledAtStartup()
  }

  shouldShowRestartWindowsRecall_(disabled: boolean) {
    return disabled != this.browserProxy_.wasWindowsRecallDisabledAtStartup()
  }

  windowsRecallDisabledChange_(event: Event) {
    const target = event.target
    assert(target instanceof SettingsToggleButtonElement)
    this.browserProxy_.setWindowsRecallDisabled(target.checked)
  }

  setWindowsRecallDisabled_(disabled: boolean) {
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: disabled,
    }
    this.windowsRecallDisabledPref_ = pref
  }

  restartBrowser_(e: Event) {
    e.stopPropagation()
    window.open('chrome://restart', '_self')
  }

  private loadVoiceSwapPersonas_() {
    this.browserProxy_.getVoiceSwapPersonas().then((personas) => {
      this.voiceSwapPersonaOptions_ =
        personas.map((p: VoiceSwapPersona) => ({ value: p.id, name: p.name }))
      this.personasById_ = new Map(personas.map((p) => [p.id, p]))
    })
  }

  private onVoiceSwapSelectionChanged_(
    _options: Object[], selectedId: string
  ) {
    const persona = this.personasById_?.get(selectedId)
    this.selectedPersonaIsCustom_ = !!persona && !persona.builtin
  }

  private onShowAddPersonaForm_() {
    this.showAddPersonaForm_ = true
    this.newPersonaName_ = ''
    this.addPersonaError_ = false
    this.newPersonaModelFile_ = null
    this.newPersonaIndexFile_ = null
    this.canSavePersona_ = false
  }

  private onCancelAddPersona_() {
    this.showAddPersonaForm_ = false
  }

  private updateCanSavePersona_() {
    this.canSavePersona_ =
      !!this.newPersonaName_?.trim() && !!this.newPersonaModelFile_
  }

  private onPersonaModelFileChanged_(e: Event) {
    const input = e.target as HTMLInputElement
    this.newPersonaModelFile_ = input.files?.[0] ?? null
    this.updateCanSavePersona_()
  }

  private onPersonaIndexFileChanged_(e: Event) {
    const input = e.target as HTMLInputElement
    this.newPersonaIndexFile_ = input.files?.[0] ?? null
  }

  private readFileAsBase64_(file: File): Promise<string> {
    return new Promise((resolve, reject) => {
      const reader = new FileReader()
      reader.onload = () => {
        // reader.result is "data:<mime>;base64,<data>" for readAsDataURL.
        const result = reader.result as string
        resolve(result.substring(result.indexOf(',') + 1))
      }
      reader.onerror = () => reject(reader.error)
      reader.readAsDataURL(file)
    })
  }

  private async onSaveVoiceSwapPersona_() {
    if (!this.newPersonaModelFile_) {
      return
    }
    this.addPersonaError_ = false
    try {
      const pthBase64 = await this.readFileAsBase64_(this.newPersonaModelFile_)
      const indexBase64 = this.newPersonaIndexFile_
        ? await this.readFileAsBase64_(this.newPersonaIndexFile_)
        : ''
      const personas = await this.browserProxy_.addVoiceSwapPersona(
        this.newPersonaName_.trim(), pthBase64, indexBase64)
      this.voiceSwapPersonaOptions_ =
        personas.map((p: VoiceSwapPersona) => ({ value: p.id, name: p.name }))
      this.personasById_ = new Map(personas.map((p) => [p.id, p]))
      this.showAddPersonaForm_ = false
    } catch (e) {
      this.addPersonaError_ = true
    }
  }

  private onRemoveVoiceSwapPersona_() {
    const id = this.getPref('brave.privacy.voice_swap_persona_id').value as string
    this.browserProxy_.removeVoiceSwapPersona(id).then((personas) => {
      this.voiceSwapPersonaOptions_ =
        personas.map((p: VoiceSwapPersona) => ({ value: p.id, name: p.name }))
      this.personasById_ = new Map(personas.map((p) => [p.id, p]))
      // Fall back to whichever persona is now first (a built-in, always
      // present) rather than leaving the pref pointing at the one just
      // removed.
      if (personas.length) {
        this.setPrefValue('brave.privacy.voice_swap_persona_id', personas[0].id)
      }
    })
  }

  // Expressed positively because Polymer cannot negate a method call inside a
  // binding, and the coordinate fields are hidden for every preset place.
  private isPresetPlace_(place: string): boolean {
    return place !== 'custom'
  }

  /**
   * Keeps the latitude/longitude text fields in step with the stored prefs.
   * The first time this runs it also selects the preset matching those
   * coordinates, so a returning user sees the place they picked rather than
   * "Custom coordinates…". After that the dropdown is driven by the user, so
   * it is left alone — otherwise typing coordinates that happen to coincide
   * with a city would make the menu jump under the cursor.
   */
  private onFakeLocationCoordsChanged_(latitude: number, longitude: number) {
    if (latitude === undefined || longitude === undefined) {
      return
    }
    this.latitudeInput_ = String(latitude)
    this.longitudeInput_ = String(longitude)

    // While the location follows the IP, coordinates are rewritten behind the
    // user's back (on VPN connect, say), so the shown place has to be
    // re-derived each time rather than only seeded once.
    const autoIp = Boolean(
      this.getPref('brave.privacy.fake_geolocation_auto_ip')?.value,
    )
    if (this.placeInitialized_ && !autoIp) {
      return
    }
    this.placeInitialized_ = true
    const match = (this.fakeLocationPlaces_ as Array<{ value: string }>).find(
      (place) => {
        if (place.value === 'custom') {
          return false
        }
        const [presetLat, presetLng] = place.value.split(',').map(Number)
        return (
          Math.abs(presetLat - latitude) < 1e-6 &&
          Math.abs(presetLng - longitude) < 1e-6
        )
      },
    )
    this.selectedPlace_ = match ? match.value : 'custom'
  }

  private onFakeLocationPlaceChanged_(e: Event) {
    const select = e.target as HTMLSelectElement
    this.selectedPlace_ = select.value
    if (select.value === 'custom') {
      // Keep whatever coordinates are already stored so the fields the user
      // is about to edit start from the last place, not from 0,0.
      return
    }
    const [latitude, longitude] = select.value.split(',').map(Number)
    this.latitudeInvalid_ = false
    this.longitudeInvalid_ = false
    this.setPrefValue('brave.privacy.fake_geolocation_latitude', latitude)
    this.setPrefValue('brave.privacy.fake_geolocation_longitude', longitude)
  }

  private onLatitudeChanged_(e: Event) {
    const value = (e.target as HTMLInputElement).value
    const latitude = Number(value)
    const valid =
      value.trim() !== '' &&
      Number.isFinite(latitude) &&
      latitude >= -90 &&
      latitude <= 90
    this.latitudeInvalid_ = !valid
    if (valid) {
      this.setPrefValue('brave.privacy.fake_geolocation_latitude', latitude)
    }
  }

  private onLongitudeChanged_(e: Event) {
    const value = (e.target as HTMLInputElement).value
    const longitude = Number(value)
    const valid =
      value.trim() !== '' &&
      Number.isFinite(longitude) &&
      longitude >= -180 &&
      longitude <= 180
    this.longitudeInvalid_ = !valid
    if (valid) {
      this.setPrefValue('brave.privacy.fake_geolocation_longitude', longitude)
    }
  }

  override ready() {
    super.ready()
    this.loadVoiceSwapPersonas_()
    // Add hr to the "Do not track" row.
    const doNotTrack = this.shadowRoot?.querySelector('#doNotTrack')
    if (doNotTrack) {
      const toggle = doNotTrack.shadowRoot?.querySelector('#toggle')
      if (toggle) {
        const toggleClass = toggle.getAttribute('class')
        toggle.setAttribute('class', toggleClass + ' hr')
      } else {
        console.log(
          '[Brave Settings Overrides] Could not find doNotTrack toggle',
        )
      }
    } else {
      console.log(
        '[Brave Settings Overrides] Could not find element with id doNotTrack',
      )
    }

    if (this.isWindowsRecallAvailable_) {
      this.addWebUiListener(
        'windows-recall-disabled-changed',
        this.setWindowsRecallDisabled_.bind(this),
      )
      this.browserProxy_
        .isWindowsRecallDisabled()
        .then(this.setWindowsRecallDisabled_.bind(this))
    }
  }
}

customElements.define(
  SettingsBravePersonalizationOptions.is,
  SettingsBravePersonalizationOptions,
)
