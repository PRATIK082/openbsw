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
#include "gwbridge/EthIoBridge.h"

namespace gwbridge
{

void EthIoBridge::bind(::commgateway::EthIpduManager& manager)
{
    manager.registerPduCallback(
        [this](uint32_t pduId, uint16_t length, uint8_t const* data) {
            onPduReceived(pduId, length, data);
        });
}

uint32_t EthIoBridge::pumpTx(::commgateway::EthIpduManager& manager)
{
    uint32_t sent = 0U;
    while (m_bridge.drainTxFrame(
        [&manager, &sent](uint32_t messageId, uint16_t length, uint8_t const* data) {
            return manager.transmitPdu(messageId, length, data);
        }))
    {
        sent++;
    }
    m_txSentCount += sent;
    return sent;
}

void EthIoBridge::onPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data)
{
    (void)m_bridge.pushRxFrame(pduId, data, length);
}

::io::IReader& EthIoBridge::rxReader() { return m_bridge.rxReader(); }

::io::IWriter& EthIoBridge::txWriter() { return m_bridge.txWriter(); }

uint32_t EthIoBridge::getRxDropCount() const { return m_bridge.getRxDropCount(); }

uint32_t EthIoBridge::getTxDropCount() const { return m_bridge.getTxDropCount(); }

uint32_t EthIoBridge::getTxSentCount() const { return m_txSentCount; }

} // namespace gwbridge
