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
 * \ingroup canstack
 */
#pragma once

#include "canstack/SignalDb.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace canstack
{

/// Transmit function: (channelId, frameId, dlc, data) -> accepted.
using TransmitFunction = std::function<bool(uint8_t, uint32_t, uint8_t, uint8_t const*)>;

/// Optional signal transformation: physical value in, physical value out.
using SignalTransform = std::function<double(double)>;

/// One routing rule: input channel + frame id to output destinations.
struct RoutingRule
{
    uint8_t inputChannel = 0U;
    uint32_t inputFrameId = 0U;
    /// (output channel, output frame id) pairs
    std::vector<std::pair<uint8_t, uint32_t>> outputDestinations;
    /// Optional transformation applied to every forwarded signal value; a
    /// transform returning NaN drops the signal.
    SignalTransform transform;
};

/**
 * PDU router across CAN channels (gateway core).
 *
 * Two forwarding modes per output destination:
 *
 * - Frame forwarding: when output frame id equals the input frame id and the
 *   rule has no transform, the payload is copied unmodified.
 *
 * - Signal gateway: otherwise the input frame is unpacked with the signal
 *   database and every signal that exists (by name) in the output frame is
 *   packed into the output frame, applying the optional transform. Signals
 *   missing in the output frame are dropped; output signals missing in the
 *   input frame keep their bits unset.
 */
class CanRouter
{
public:
    /// \param db signal database used for signal level routing
    void setDatabase(SignalDatabase const* db);

    /// \param transmit transmit function used to forward frames (typically
    /// CanInterface::sendFrame bound to the channel manager)
    void setTransmitFunction(TransmitFunction transmit);

    void addRoutingRule(RoutingRule const& rule);

    /// Removes all routing rules.
    void clearRules();

    /**
     * Feeds a received frame into the routing table.
     * Called from the stack's receive path for every frame.
     */
    void onFrameReceived(uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data);

    size_t getRuleCount() const;

private:
    void forwardFrameLevel(
        RoutingRule const& rule, uint8_t dlc, uint8_t const* data);

    void forwardSignalLevel(
        RoutingRule const& rule, uint8_t inputChannel, uint32_t inputFrameId, uint8_t dlc,
        uint8_t const* data);

    std::vector<RoutingRule> m_routingTable;
    SignalDatabase const* m_signalDb = nullptr;
    TransmitFunction m_transmit;
};

} // namespace canstack
