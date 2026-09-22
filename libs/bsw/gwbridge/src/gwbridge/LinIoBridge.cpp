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
#include "gwbridge/LinIoBridge.h"

namespace gwbridge
{

void LinIoBridge::bind(::commgateway::LinChannel& channel)
{
    channel.registerUpperLayerCallback(
        [this](uint8_t channelId, uint8_t pid, uint8_t* data, uint8_t dlc) {
            onFrameReceived(channelId, pid, data, dlc);
        });
}

uint32_t LinIoBridge::pumpTx(::commgateway::LinChannel& channel)
{
    uint32_t sent = 0U;
    while (m_bridge.drainTxFrame(
        [&channel, &sent](uint32_t messageId, uint16_t length, uint8_t const* data) {
            if ((length == 0U) || (length > 8U) || (messageId > 0xFFU))
            {
                return false;
            }
            uint8_t buffer[8U] = {0U};
            for (uint16_t i = 0U; i < length; ++i)
            {
                buffer[i] = data[i];
            }
            return channel.transmitFrame(static_cast<uint8_t>(messageId), buffer,
                                         static_cast<uint8_t>(length));
        }))
    {
        sent++;
    }
    m_txSentCount += sent;
    return sent;
}

void LinIoBridge::onFrameReceived(uint8_t /* channelId */, uint8_t pid, uint8_t* data,
                                  uint8_t dlc)
{
    (void)m_bridge.pushRxFrame(pid, data, dlc);
}

::io::IReader& LinIoBridge::rxReader() { return m_bridge.rxReader(); }

::io::IWriter& LinIoBridge::txWriter() { return m_bridge.txWriter(); }

uint32_t LinIoBridge::getRxDropCount() const { return m_bridge.getRxDropCount(); }

uint32_t LinIoBridge::getTxDropCount() const { return m_bridge.getTxDropCount(); }

uint32_t LinIoBridge::getTxSentCount() const { return m_txSentCount; }

} // namespace gwbridge
