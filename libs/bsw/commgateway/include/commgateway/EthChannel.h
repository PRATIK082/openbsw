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

#include "commgateway/EthIpdu.h"

#include <cstdint>
#include <vector>

namespace commgateway
{

/**
 * In-memory EthTransportIf for tests and POSIX simulation.
 *
 * Records every sent datagram; with loopback enabled sent datagrams are
 * redelivered to the rx callback (source port preserved). injectRx() feeds
 * arbitrary inbound traffic.
 */
class EthLoopbackChannel : public EthTransportIf
{
public:
    struct Datagram
    {
        uint32_t destIp;
        uint16_t destPort;
        uint16_t srcPort;
        std::vector<uint8_t> payload;
    };

    EthLoopbackChannel() = default;

    bool send(uint32_t destIp, uint16_t destPort, uint16_t srcPort, uint8_t const* data,
              uint16_t length) override;
    void registerRxCallback(RxCallback cb) override;

    void injectRx(uint32_t srcIp, uint16_t srcPort, uint8_t const* data, uint16_t length);
    void setLoopback(bool enabled);

    std::vector<Datagram> const& getTxLog() const;
    void clearTxLog();

private:
    RxCallback m_rxCallback;
    std::vector<Datagram> m_txLog;
    bool m_loopback = true;
};

} // namespace commgateway
