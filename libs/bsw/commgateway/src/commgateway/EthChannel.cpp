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
 * \ingroup commgateway
 */
#include "commgateway/EthChannel.h"

namespace commgateway
{

bool EthLoopbackChannel::send(uint32_t destIp, uint16_t destPort, uint16_t srcPort,
                              uint8_t const* data, uint16_t length)
{
    if (data == nullptr)
    {
        return false;
    }
    Datagram datagram{};
    datagram.destIp   = destIp;
    datagram.destPort = destPort;
    datagram.srcPort  = srcPort;
    datagram.payload.assign(data, data + length);
    m_txLog.push_back(datagram);
    if (m_loopback && m_rxCallback)
    {
        m_rxCallback(destIp, srcPort, datagram.payload.data(),
                     static_cast<uint16_t>(datagram.payload.size()));
    }
    return true;
}

void EthLoopbackChannel::registerRxCallback(RxCallback cb) { m_rxCallback = cb; }

void EthLoopbackChannel::injectRx(uint32_t srcIp, uint16_t srcPort, uint8_t const* data,
                                  uint16_t length)
{
    if (m_rxCallback && (data != nullptr))
    {
        m_rxCallback(srcIp, srcPort, data, length);
    }
}

void EthLoopbackChannel::setLoopback(bool enabled) { m_loopback = enabled; }

std::vector<EthLoopbackChannel::Datagram> const& EthLoopbackChannel::getTxLog() const
{
    return m_txLog;
}

void EthLoopbackChannel::clearTxLog() { m_txLog.clear(); }

} // namespace commgateway
