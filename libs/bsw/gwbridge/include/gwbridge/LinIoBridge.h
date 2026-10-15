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
 * Reuse note: binds the Phase 2 commgateway::LinChannel (public API only).
 * There is no libs/bsw/lin LinController in OpenBSW (verified).
 */
#pragma once

#include "gwbridge/IoBridge.h"

#include "commgateway/LinChannel.h"

#include <cstdint>

namespace gwbridge
{

/// RX/TX queues: 512 B / 16 B elements (8 B header + 8 B LIN payload).
using LinQueueBridge = IoBridge<512U, 16U, 512U, 16U>;

/**
 * Bridge between a Phase 2 LinChannel and the routing module.
 * The LIN PID serves as routing message id.
 */
class LinIoBridge
{
public:
    LinIoBridge() = default;

    LinIoBridge(LinIoBridge const&)            = delete;
    LinIoBridge& operator=(LinIoBridge const&) = delete;

    void bind(::commgateway::LinChannel& channel);
    uint32_t pumpTx(::commgateway::LinChannel& channel);

    void onFrameReceived(uint8_t channelId, uint8_t pid, uint8_t* data, uint8_t dlc);

    ::io::IReader& rxReader();
    ::io::IWriter& txWriter();
    uint32_t getRxDropCount() const;
    uint32_t getTxDropCount() const;
    uint32_t getTxSentCount() const;

private:
    LinQueueBridge m_bridge;
    uint32_t m_txSentCount = 0U;
};

} // namespace gwbridge
