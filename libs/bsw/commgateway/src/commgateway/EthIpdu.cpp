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
#include "commgateway/EthIpdu.h"

namespace commgateway
{

void EthIpduManager::init(EthIpduConfig const& config, EthTransportIf* transport)
{
    m_config      = config;
    m_transport   = transport;
    m_initialized = (transport != nullptr);
    if (m_initialized)
    {
        m_transport->registerRxCallback([this](uint32_t srcIp, uint16_t srcPort,
                                               uint8_t const* data, uint16_t length) {
            onDatagramReceived(srcIp, srcPort, data, length);
        });
    }
}

void EthIpduManager::shutdown()
{
    m_initialized = false;
    m_transport   = nullptr;
    m_txBuffers.clear();
}

bool EthIpduManager::transmitPdu(uint32_t pduId, uint16_t length, uint8_t const* data)
{
    if (!m_initialized || (m_transport == nullptr) || (data == nullptr))
    {
        return false;
    }
    std::vector<uint8_t> payload(data, data + length);
    m_txBuffers[pduId] = payload;
    if (!m_transport->send(m_config.destIp, m_config.destPort, m_config.srcPort, data, length))
    {
        return false;
    }
    m_txCount++;
    return true;
}

void EthIpduManager::onDatagramReceived(uint32_t /* srcIp */, uint16_t srcPort,
                                        uint8_t const* data, uint16_t length)
{
    auto it    = m_portToPdu.find(srcPort);
    uint32_t pduId = (it != m_portToPdu.end()) ? it->second : m_config.primaryPduId;
    onPduReceived(pduId, length, data);
}

void EthIpduManager::onPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data)
{
    m_rxCount++;
    if (m_pduCallback)
    {
        m_pduCallback(pduId, length, data);
    }
}

void EthIpduManager::registerPduCallback(EthPduCallback cb) { m_pduCallback = cb; }

void EthIpduManager::registerPduMapping(uint16_t srcPort, uint32_t pduId)
{
    m_portToPdu[srcPort] = pduId;
}

void EthIpduManager::mainFunction(uint32_t nowMs)
{
    if (!m_initialized || (m_config.cycleTimeMs == 0U))
    {
        return;
    }
    if ((nowMs - m_lastTxTimeMs) < m_config.cycleTimeMs)
    {
        return;
    }
    auto it = m_txBuffers.find(m_config.primaryPduId);
    if (it == m_txBuffers.end())
    {
        return;
    }
    std::vector<uint8_t> const payload = it->second;
    if (m_transport->send(m_config.destIp, m_config.destPort, m_config.srcPort, payload.data(),
                          static_cast<uint16_t>(payload.size())))
    {
        m_lastTxTimeMs = nowMs;
        m_txCount++;
    }
}

bool EthIpduManager::isInitialized() const { return m_initialized; }

EthIpduConfig const& EthIpduManager::getConfig() const { return m_config; }

uint32_t EthIpduManager::getTxCount() const { return m_txCount; }

uint32_t EthIpduManager::getRxCount() const { return m_rxCount; }

} // namespace commgateway
