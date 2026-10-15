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
 * Reuse note: binds the Phase 1 canstack::CanChannel (used via its public
 * API only) to an IoBridge. There is no libs/bsw/can CanController in
 * OpenBSW (verified), so no such dependency is introduced.
 */
#pragma once

#include "gwbridge/IoBridge.h"

#include "canstack/CanChannel.h"

#include <cstdint>

namespace gwbridge
{

/// RX queue: 2048 B / 80 B elements (8 B header + 64 B CAN FD + margin).
/// TX queue: same sizing for symmetric paths.
using CanQueueBridge = IoBridge<2048U, 80U, 2048U, 80U>;

/**
 * Bridge between a Phase 1 CanChannel and the routing module.
 *
 * RX: the channel upper-layer callback pushes [id][len][payload] messages.
 * TX: pumpTx() drains queued messages into CanChannel::transmitFrame().
 */
class CanIoBridge
{
public:
    CanIoBridge() = default;

    CanIoBridge(CanIoBridge const&)            = delete;
    CanIoBridge& operator=(CanIoBridge const&) = delete;

    /// Registers the RX callback on an initialized channel.
    void bind(::canstack::CanChannel& channel);
    /// Drains all queued TX messages; \return number of accepted frames.
    uint32_t pumpTx(::canstack::CanChannel& channel);

    void onFrameReceived(uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data);

    ::io::IReader& rxReader();
    ::io::IWriter& txWriter();
    uint32_t getRxDropCount() const;
    uint32_t getTxDropCount() const;
    uint32_t getTxSentCount() const;

private:
    CanQueueBridge m_bridge;
    uint32_t m_txSentCount = 0U;
};

} // namespace gwbridge
