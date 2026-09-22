/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * \file
 * \ingroup gwbridge
 */
#include "gwbridge/CanIoBridge.h"

namespace gwbridge
{

void CanIoBridge::bind(::canstack::CanChannel& channel)
{
    channel.registerUpperLayerCallback(
        [this](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
            onFrameReceived(channelId, frameId, dlc, data);
        });
}

uint32_t CanIoBridge::pumpTx(::canstack::CanChannel& channel)
{
    uint32_t sent = 0U;
    while (m_bridge.drainTxFrame(
        [&channel, &sent](uint32_t messageId, uint16_t length, uint8_t const* data) {
            if (length > 255U)
            {
                return false;
            }
            return channel.transmitFrame(messageId, static_cast<uint8_t>(length), data);
        }))
    {
        sent++;
    }
    m_txSentCount += sent;
    return sent;
}

void CanIoBridge::onFrameReceived(uint8_t /* channelId */, uint32_t frameId, uint8_t dlc,
                                  uint8_t const* data)
{
    (void)m_bridge.pushRxFrame(frameId, data, dlc);
}

::io::IReader& CanIoBridge::rxReader() { return m_bridge.rxReader(); }

::io::IWriter& CanIoBridge::txWriter() { return m_bridge.txWriter(); }

uint32_t CanIoBridge::getRxDropCount() const { return m_bridge.getRxDropCount(); }

uint32_t CanIoBridge::getTxDropCount() const { return m_bridge.getTxDropCount(); }

uint32_t CanIoBridge::getTxSentCount() const { return m_txSentCount; }

} // namespace gwbridge
