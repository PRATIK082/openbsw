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
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace commgateway
{

/// Injectable UDP/TCP transport; production code binds this to cpp2ethernet
/// sockets, tests use EthLoopbackChannel. Keeps this module stdlib-only.
class EthTransportIf
{
public:
    /// Incoming datagram: (sourceIp, sourcePort, data, length).
    using RxCallback = std::function<void(uint32_t, uint16_t, uint8_t const*, uint16_t)>;

    virtual ~EthTransportIf() = default;

    EthTransportIf(EthTransportIf const&)            = delete;
    EthTransportIf& operator=(EthTransportIf const&) = delete;

    virtual bool send(uint32_t destIp, uint16_t destPort, uint16_t srcPort, uint8_t const* data,
                      uint16_t length)
        = 0;
    virtual void registerRxCallback(RxCallback cb) = 0;

protected:
    EthTransportIf() = default;
};

/// Ethernet I-PDU configuration (UDP/TCP/SOME-IP payload mapping).
struct EthIpduConfig
{
    uint16_t ethChannelId = 0U;
    uint16_t ethertype    = 0x0800U;
    uint32_t destIp       = 0U;
    uint16_t destPort     = 0U;
    uint16_t srcPort      = 0U;
    /// Retransmission period of the last frame; 0 = on-demand only.
    uint32_t cycleTimeMs  = 0U;
    /// PDU carried on this manager's ports.
    uint32_t primaryPduId = 0U;
    std::vector<uint32_t> associatedPduIds;
};

/// PDU notification: (pduId, length, data).
using EthPduCallback = std::function<void(uint32_t, uint16_t, uint8_t const*)>;

/**
 * Ethernet I-PDU manager: maps PDU ids to UDP datagrams.
 *
 * Datagrams arriving on the bound transport are reported as primaryPduId
 * unless a source-port mapping was registered via registerPduMapping().
 * With cycleTimeMs > 0 the last transmitted frame is repeated cyclically.
 */
class EthIpduManager
{
public:
    EthIpduManager() = default;

    void init(EthIpduConfig const& config, EthTransportIf* transport);
    void shutdown();

    bool transmitPdu(uint32_t pduId, uint16_t length, uint8_t const* data);
    void onDatagramReceived(uint32_t srcIp, uint16_t srcPort, uint8_t const* data, uint16_t length);
    void onPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data);
    void registerPduCallback(EthPduCallback cb);
    /// Maps an additional source port to a PDU id for rx demultiplexing.
    void registerPduMapping(uint16_t srcPort, uint32_t pduId);

    /// Handles cycle-time retransmission; call every millisecond.
    void mainFunction(uint32_t nowMs);

    bool isInitialized() const;
    EthIpduConfig const& getConfig() const;
    uint32_t getTxCount() const;
    uint32_t getRxCount() const;

private:
    EthIpduConfig m_config{};
    EthTransportIf* m_transport  = nullptr;
    EthPduCallback m_pduCallback;
    std::map<uint16_t, uint32_t> m_portToPdu;
    std::map<uint32_t, std::vector<uint8_t>> m_txBuffers;
    uint32_t m_lastTxTimeMs = 0U;
    uint32_t m_txCount      = 0U;
    uint32_t m_rxCount      = 0U;
    bool m_initialized      = false;
};

} // namespace commgateway
