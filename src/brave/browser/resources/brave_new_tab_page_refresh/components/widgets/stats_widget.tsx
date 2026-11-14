/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'
import Icon from '@brave/leo/react/icon'

import {
  GeolocationMode,
  PrivacyStatus,
} from '../../state/new_tab_store'
import { useNewTabState, useNewTabActions } from '../../context/new_tab_context'
import { getString } from '../../lib/strings'
import { WidgetMenu } from './widget_menu'

import { style } from './stats_widget.style'

const adsBlockedFormatter = new Intl.NumberFormat(undefined, {
  maximumFractionDigits: 0,
  useGrouping: true,
})

function formatLocationStatus(status: PrivacyStatus): string {
  switch (status.geolocationMode) {
    case GeolocationMode.kBlocked:
      return 'Blocked'
    case GeolocationMode.kFake: {
      const coords = `${status.fakeLatitude.toFixed(4)}, ${status.fakeLongitude.toFixed(4)}`
      return status.fakeGeolocationAutoIp
        ? `Fake (${coords}, Auto-IP)`
        : `Fake (${coords})`
    }
    case GeolocationMode.kReal:
    default:
      return 'Real Location'
  }
}

export function StatsWidget() {
  const showStats = useNewTabState((s) => s.showShieldsStats)
  const stats = useNewTabState((s) => s.shieldsStats)
  const privacy = useNewTabState((s) => s.privacyStatus)
  const actions = useNewTabActions()

  function renderUnits(parts: Intl.NumberFormatPart[]) {
    return parts.map(({ type, value }) => {
      if (type === 'unit') {
        return (
          <span
            key='unit'
            className='units'
          >
            {value}
          </span>
        )
      }
      return value
    })
  }

  if (!showStats) {
    return null
  }

  const fakeTzText =
    privacy && privacy.fakeTimezoneEnabled && privacy.fakeTimezoneId
      ? `${privacy.fakeTimezoneId}${privacy.fakeTimezoneAutoIp ? ' (Auto)' : ''}`
      : 'Off'

  return (
    <div data-css-scope={style.scope}>
      <WidgetMenu>
        <leo-menu-item onClick={() => actions.setShowShieldsStats(false)}>
          <Icon name='eye-off' /> {getString(S.NEW_TAB_HIDE_STATS_WIDGET_LABEL)}
        </leo-menu-item>
      </WidgetMenu>
      <div className='card-body'>
        <div className='stats-section'>
          <div className='title'>ANAR SHIELDS</div>
          <div className='data'>
            <div>
              <div className='ads-blocked'>
                <div className='value'>
                  {stats && adsBlockedFormatter.format(stats.adsBlocked)}
                </div>
                {getString(S.NEW_TAB_STATS_ADS_BLOCKED_TEXT)}
              </div>
              <div className='bandwidth-saved'>
                <div className='value'>
                  {stats && renderUnits(formatBandwidth(stats.bandwidthSavedBytes))}
                </div>
                {getString(S.NEW_TAB_STATS_BANDWIDTH_SAVED_TEXT)}
              </div>
              <div className='time-saved'>
                <div className='value'>
                  {stats &&
                    renderUnits(formatTimeSaved(getTimeSaved(stats.adsBlocked)))}
                </div>
                Seconds saved
              </div>
            </div>
          </div>
        </div>

        <div
          className='vertical-divider'
          aria-hidden='true'
        />

        <div className='privacy-section'>
          <div className='privacy-grid'>
            <div className='privacy-item'>
              <span className='privacy-label'>IP Address</span>
              <span className='privacy-value'>
                {privacy?.ipAddress || 'Checking...'}
              </span>
            </div>
            <div className='privacy-item'>
              <span className='privacy-label'>VPN Status</span>
              <span
                className={
                  privacy?.vpnConnected
                    ? 'privacy-value vpn-on'
                    : 'privacy-value vpn-off'
                }
              >
                <span className='status-dot' />
                {privacy ? privacy.vpnStatus : 'Checking...'}
              </span>
            </div>
            <div className='privacy-item'>
              <span className='privacy-label'>Fake Time Zone</span>
              <span className='privacy-value'>
                {privacy ? fakeTzText : '—'}
              </span>
            </div>
            <div className='privacy-item'>
              <span className='privacy-label'>Real Time Zone</span>
              <span className='privacy-value'>
                {privacy?.realTimezoneId || '—'}
              </span>
            </div>
            <div className='privacy-item privacy-item-wide'>
              <span className='privacy-label'>GPS Location</span>
              <span className='privacy-value'>
                {privacy ? formatLocationStatus(privacy) : '—'}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}

function getTimeSaved(adsBlocked: number) {
  return adsBlocked * 50
}

function formatTimeInterval(
  value: number,
  unit: 'day' | 'hour' | 'minute' | 'second',
  maximumFractionDigits: number = 0,
) {
  return new Intl.NumberFormat(undefined, {
    style: 'unit',
    unit,
    unitDisplay: 'long',
    maximumFractionDigits,
    roundingMode: 'ceil',
  }).formatToParts(value)
}

function formatTimeSaved(ms: number) {
  const seconds = ms / 1000
  const minutes = seconds / 60
  const hours = minutes / 60
  const days = hours / 24

  if (days >= 1) {
    return formatTimeInterval(days, 'day', 2)
  }
  if (hours >= 1) {
    return formatTimeInterval(hours, 'hour', 1)
  }
  if (minutes >= 1) {
    return formatTimeInterval(minutes, 'minute')
  }
  if (seconds >= 1) {
    return formatTimeInterval(seconds, 'second')
  }
  return formatTimeInterval(0, 'second')
}

function formatMemoryValue(
  value: number,
  unit: 'byte' | 'kilobyte' | 'megabyte' | 'gigabyte',
  maximumFractionDigits: number = 0,
) {
  return new Intl.NumberFormat(undefined, {
    style: 'unit',
    unit,
    unitDisplay: 'narrow',
    maximumFractionDigits,
  }).formatToParts(value)
}

function formatBandwidth(bytes: number) {
  const kb = bytes / 1024
  const mb = kb / 1024
  const gb = mb / 1024

  if (gb >= 1) {
    return formatMemoryValue(gb, 'gigabyte', 2)
  }
  if (mb >= 1) {
    return formatMemoryValue(mb, 'megabyte', 1)
  }
  if (kb >= 1) {
    return formatMemoryValue(kb, 'kilobyte')
  }
  if (bytes === 0) {
    return formatMemoryValue(0, 'kilobyte')
  }
  return formatMemoryValue(bytes, 'byte')
}
