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
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace framegateway
{

/// DoIP payload types (ISO 13400-2, subset).
static uint16_t const DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ = 0x0005U;
static uint16_t const DOIP_PAYLOAD_ROUTING_ACTIVATION_RES = 0x0006U;
static uint16_t const DOIP_PAYLOAD_UDS_MESSAGE            = 0x8001U;
static uint16_t const DOIP_PAYLOAD_UDS_ACK                = 0x8002U;
/// DoIP port per ISO 13400.
static uint16_t const DOIP_DEFAULT_PORT = 13400U;

/// Ethernet-based diagnostics endpoint configuration.
struct DoIpConfig
{
    uint16_t ethChannelId = 0U;
    uint32_t destIp       = 0U;
    uint16_t destPort     = DOIP_DEFAULT_PORT;
    uint16_t srcPort      = DOIP_DEFAULT_PORT;
    uint32_t vin          = 0U;
};

/// Raw DoIP datagram transmit hook: (length, data) -> accepted.
using DoIpTxSender = std::function<bool(uint16_t, uint8_t const*)>;
/// Extracted UDS payload sink: (sourceAddress, targetAddress, length, data).
using DoIpUdsCallback = std::function<void(uint16_t, uint16_t, uint16_t, uint8_t const*)>;

/**
 * DoIP (Diagnostics over IP) handler: header codec plus routing-activation
 * and UDS-message dispatch.
 *
 * Transport is an injected datagram sender (UDP via EthIpdu in production,
 * loopback in tests), so there is no hard dependency on the Ethernet stack.
 */
class DoIpHandler
{
public:
    DoIpHandler() = default;

    void init(DoIpConfig const& config);
    void shutdown();

    void onDoIpMessageReceived(uint8_t const* data, uint16_t length);
    bool sendDoIpMessage(uint16_t payloadType, uint8_t const* payload, uint16_t payloadLength);
    /// Sends a UDS request payload with source/target addresses.
    bool sendUdsMessage(uint16_t sourceAddress, uint16_t targetAddress, uint8_t const* udsData,
                        uint16_t udsLength);

    void setTxSender(DoIpTxSender sender);
    void setUdsCallback(DoIpUdsCallback cb);

    bool isRoutingActive() const;
    DoIpConfig const& getConfig() const;
    uint32_t getRxCount() const;
    uint32_t getTxCount() const;

private:
    bool handleRoutingActivation(uint8_t const* payload, uint16_t payloadLength);
    bool handleUdsMessage(uint8_t const* payload, uint16_t payloadLength);

    DoIpConfig m_config{};
    DoIpTxSender m_txSender;
    DoIpUdsCallback m_udsCallback;
    std::vector<uint8_t> m_txBuffer;
    bool m_routingActive = false;
    uint32_t m_rxCount   = 0U;
    uint32_t m_txCount   = 0U;
    bool m_initialized   = false;
};

} // namespace framegateway
