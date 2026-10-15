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
 *
 * Reuse note: binds the Phase 2 commgateway::EthIpduManager (public API
 * only). The PDU id serves as routing message id.
 */
#pragma once

#include "gwbridge/IoBridge.h"

#include "commgateway/EthIpdu.h"

#include <cstdint>

namespace gwbridge
{

/// RX/TX queues: 4096 B / 1520 B elements (8 B header + ETH MTU payload).
using EthQueueBridge = IoBridge<4096U, 1520U, 4096U, 1520U>;

/**
 * Bridge between a Phase 2 EthIpduManager and the routing module.
 */
class EthIoBridge
{
public:
    EthIoBridge() = default;

    EthIoBridge(EthIoBridge const&)            = delete;
    EthIoBridge& operator=(EthIoBridge const&) = delete;

    void bind(::commgateway::EthIpduManager& manager);
    uint32_t pumpTx(::commgateway::EthIpduManager& manager);

    void onPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data);

    ::io::IReader& rxReader();
    ::io::IWriter& txWriter();
    uint32_t getRxDropCount() const;
    uint32_t getTxDropCount() const;
    uint32_t getTxSentCount() const;

private:
    EthQueueBridge m_bridge;
    uint32_t m_txSentCount = 0U;
};

} // namespace gwbridge
