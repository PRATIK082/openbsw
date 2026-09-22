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
 * \ingroup framegateway
 */
#include "framegateway/DoIpHandler.h"

#include <cstring>

namespace framegateway
{
namespace
{

uint16_t readBigEndian16(uint8_t const* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) | data[1]);
}

uint32_t readBigEndian32(uint8_t const* data)
{
    return (static_cast<uint32_t>(data[0]) << 24U) | (static_cast<uint32_t>(data[1]) << 16U)
           | (static_cast<uint32_t>(data[2]) << 8U) | static_cast<uint32_t>(data[3]);
}

void writeBigEndian16(uint8_t* data, uint16_t value)
{
    data[0] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[1] = static_cast<uint8_t>(value & 0xFFU);
}

void writeBigEndian32(uint8_t* data, uint32_t value)
{
    data[0] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    data[1] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[2] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[3] = static_cast<uint8_t>(value & 0xFFU);
}

uint8_t const DOIP_HEADER_SIZE       = 8U;
uint8_t const DOIP_PROTOCOL_VERSION  = 0x02U;

} // namespace

void DoIpHandler::init(DoIpConfig const& config)
{
    m_config       = config;
    m_routingActive = false;
    m_rxCount      = 0U;
    m_txCount      = 0U;
    m_initialized  = true;
}

void DoIpHandler::shutdown()
{
    m_initialized   = false;
    m_routingActive = false;
}

void DoIpHandler::onDoIpMessageReceived(uint8_t const* data, uint16_t length)
{
    if (!m_initialized || (data == nullptr) || (length < DOIP_HEADER_SIZE))
    {
        return;
    }
    if ((data[0] != DOIP_PROTOCOL_VERSION) || (data[1] != (uint8_t)(~DOIP_PROTOCOL_VERSION)))
    {
        return;
    }
    uint16_t const payloadType   = readBigEndian16(data + 2U);
    uint32_t const payloadLength = readBigEndian32(data + 4U);
    if ((payloadLength + DOIP_HEADER_SIZE) > length)
    {
        return;
    }
    uint8_t const* const payload = data + DOIP_HEADER_SIZE;
    m_rxCount++;

    if (payloadType == DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ)
    {
        (void)handleRoutingActivation(payload, static_cast<uint16_t>(payloadLength));
    }
    else if (payloadType == DOIP_PAYLOAD_UDS_MESSAGE)
    {
        if (m_routingActive)
        {
            (void)handleUdsMessage(payload, static_cast<uint16_t>(payloadLength));
        }
    }
}

bool DoIpHandler::sendDoIpMessage(uint16_t payloadType, uint8_t const* payload,
                                  uint16_t payloadLength)
{
    if (!m_initialized || !m_txSender || ((payload == nullptr) && (payloadLength > 0U)))
    {
        return false;
    }
    m_txBuffer.resize(DOIP_HEADER_SIZE + payloadLength);
    m_txBuffer[0] = DOIP_PROTOCOL_VERSION;
    m_txBuffer[1] = static_cast<uint8_t>(~DOIP_PROTOCOL_VERSION);
    writeBigEndian16(m_txBuffer.data() + 2U, payloadType);
    writeBigEndian32(m_txBuffer.data() + 4U, payloadLength);
    if (payloadLength > 0U)
    {
        (void)std::memcpy(m_txBuffer.data() + DOIP_HEADER_SIZE, payload, payloadLength);
    }
    if (!m_txSender(static_cast<uint16_t>(m_txBuffer.size()), m_txBuffer.data()))
    {
        return false;
    }
    m_txCount++;
    return true;
}

bool DoIpHandler::sendUdsMessage(uint16_t sourceAddress, uint16_t targetAddress,
                                 uint8_t const* udsData, uint16_t udsLength)
{
    if ((udsData == nullptr) && (udsLength > 0U))
    {
        return false;
    }
    std::vector<uint8_t> payload(4U + udsLength);
    writeBigEndian16(payload.data(), sourceAddress);
    writeBigEndian16(payload.data() + 2U, targetAddress);
    if (udsLength > 0U)
    {
        (void)std::memcpy(payload.data() + 4U, udsData, udsLength);
    }
    return sendDoIpMessage(DOIP_PAYLOAD_UDS_MESSAGE, payload.data(),
                           static_cast<uint16_t>(payload.size()));
}

void DoIpHandler::setTxSender(DoIpTxSender sender) { m_txSender = sender; }

void DoIpHandler::setUdsCallback(DoIpUdsCallback cb) { m_udsCallback = cb; }

bool DoIpHandler::isRoutingActive() const { return m_routingActive; }

DoIpConfig const& DoIpHandler::getConfig() const { return m_config; }

uint32_t DoIpHandler::getRxCount() const { return m_rxCount; }

uint32_t DoIpHandler::getTxCount() const { return m_txCount; }

bool DoIpHandler::handleRoutingActivation(uint8_t const* payload, uint16_t payloadLength)
{
    if (payloadLength < 7U)
    {
        return false;
    }
    // Accept any source address; echo it back with success code 0x00.
    uint8_t response[9U] = {0U};
    response[0] = payload[0]; // source address high
    response[1] = payload[1]; // source address low
    response[2] = 0x00U;      // activation type echo (default)
    response[3] = 0x00U;
    response[4] = 0x00U;
    response[5] = 0x00U;
    response[6] = 0x00U;
    response[7] = 0x00U;
    response[8] = 0x00U; // response code: success
    m_routingActive = true;
    return sendDoIpMessage(DOIP_PAYLOAD_ROUTING_ACTIVATION_RES, response, sizeof(response));
}

bool DoIpHandler::handleUdsMessage(uint8_t const* payload, uint16_t payloadLength)
{
    if ((payloadLength < 4U) || !m_udsCallback)
    {
        return false;
    }
    uint16_t const sourceAddress = readBigEndian16(payload);
    uint16_t const targetAddress = readBigEndian16(payload + 2U);
    m_udsCallback(sourceAddress, targetAddress, static_cast<uint16_t>(payloadLength - 4U),
                  payload + 4U);
    return true;
}

} // namespace framegateway
