/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { color, font } from '@brave/leo/tokens/css/variables'
import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  & {
    min-height: 100%;
    display: flex;
    flex-direction: column;
  }

  .card-body {
    flex: 1 1 auto;
    display: flex;
    align-items: stretch;
    gap: 20px;

    @media (width <= 760px) {
      flex-direction: column;
      gap: 14px;
    }
  }

  .stats-section {
    flex: 1 1 0;
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .title {
    font: ${font.components.buttonSmall};
    letter-spacing: 0.04em;
    color: rgba(255, 255, 255, 0.72);
  }

  .data {
    flex: 1 1 auto;
    display: flex;
    align-items: center;
    color: ${color.primitive.neutral[90]};
    font: ${font.xSmall.regular};

    > div {
      flex-grow: 1;
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 18px;
    }
  }

  .ads-blocked {
    --self-value-color: ${color.primitive.orange[70]};
  }

  .bandwidth-saved {
    --self-value-color: ${color.primitive.primary[70]};
  }

  .value {
    color: var(--self-value-color, #fff);
    font: ${font.heading.h3};
  }

  .units {
    font: ${font.default.semibold};
    text-transform: capitalize;
  }

  .vertical-divider {
    width: 1px;
    align-self: stretch;
    background: rgba(255, 255, 255, 0.16);

    @media (width <= 760px) {
      width: auto;
      height: 1px;
    }
  }

  .privacy-section {
    flex: 1.35 1 0;
    min-width: 0;
    padding-inline-end: 24px;
    display: flex;
    align-items: center;
  }

  .privacy-grid {
    width: 100%;
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 8px 16px;
  }

  .privacy-item {
    display: flex;
    flex-direction: column;
    gap: 2px;
    min-width: 0;
  }

  .privacy-item-wide {
    grid-column: 1 / -1;
  }

  .privacy-label {
    font-size: 11px;
    line-height: 14px;
    color: rgba(255, 255, 255, 0.6);
  }

  .privacy-value {
    font-size: 12px;
    line-height: 16px;
    font-weight: 500;
    color: #f2f2f2;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }

  .status-dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    flex: 0 0 auto;
    background: #8e8e93;
  }

  .vpn-on {
    color: #6ee7b7;

    .status-dot {
      background: #34d399;
      box-shadow: 0 0 6px rgba(52, 211, 153, 0.6);
    }
  }

  .vpn-off {
    color: #d1d5db;
  }
`
