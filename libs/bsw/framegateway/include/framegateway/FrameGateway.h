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

#include "framegateway/GatewayPolicy.h"
#include "framegateway/PduAssembler.h"

#include <cstdint>
#include <functional>
#include <map>
#include <tuple>
#include <vector>

namespace framegateway
{

/// What happens with extracted PDUs of a frame.
enum class GatewayAction : uint8_t
{
    ROUTE_ONLY,
    EXTRACT_AND_SIGNAL,
    BOTH
};

/// Frame layout: one bus frame carrying 1..N PDUs.
struct FrameConfig
{
    uint32_t frameId    = 0U;
    uint8_t channelType = 0U; // 0=CAN, 1=LIN, 2=ETH
    uint8_t channelId   = 0U;
    uint16_t frameLength = 8U; // total payload (8 classic CAN .. 64 CAN FD .. 1500 ETH)
    std::vector<PduInFrame> pdus;
    GatewayAction action = GatewayAction::ROUTE_ONLY;
};

/// Frame-level destination of a routed PDU.
struct GatewayDestination
{
    uint8_t channelType = 0U;
    uint8_t channelId   = 0U;
    uint32_t frameId    = 0U;
};

/// Frame transmit hook: (channelType, channelId, frameId, length, data) -> accepted.
using FrameTxSender = std::function<bool(uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*)>;
/// Extracted PDU sink (signal path): (pduId, length, data).
using PduSink = std::function<void(uint32_t, uint16_t, uint8_t const*)>;
/// Large-PDU handoff to a transport protocol: (channelType, channelId, frameId, length, data).
using TpForwarder = std::function<void(uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*)>;

/**
 * Frame/PDU-level router.
 *
 * Lower layers (CanChannel, LinChannel, EthIpdu upper callbacks) feed raw
 * frames into onFrameReceived() — no changes to Phase 1/2 modules needed.
 * Frames are policy checked, split into PDUs and then routed (frame level),
 * consumed locally (signal path), or handed to a transport protocol when a
 * PDU exceeds maxSingleFramePayload.
 */
class FrameGateway
{
public:
    FrameGateway() = default;

    void init(std::vector<FrameConfig> const& frameTable);
    void shutdown();
    void clear();

    void onFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                         uint16_t length, uint8_t const* data);
    void extractPdus(FrameConfig const& frame, uint8_t const* frameData, uint16_t length);
    void routePdu(uint32_t pduId, uint16_t length, uint8_t const* data);

    /// Registers frame-level destinations of one PDU id.
    void registerPduRoute(uint32_t pduId, std::vector<GatewayDestination> const& destinations);
    /// Buffers one PDU payload for a later packAndTransmit().
    bool storePdu(uint32_t pduId, uint16_t length, uint8_t const* data);
    /// Packs all buffered PDUs of the frame and transmits via the TxSender.
    bool packAndTransmit(uint8_t channelType, uint8_t channelId, uint32_t frameId);

    void setTxSender(FrameTxSender sender);
    void setPduSink(PduSink sink);
    void setTpForwarder(TpForwarder forwarder);
    void setPolicyEngine(PolicyEngine* policy);
    void setMaxSingleFramePayload(uint16_t payload);

    FrameConfig const* findFrame(uint8_t channelType, uint8_t channelId, uint32_t frameId) const;
    size_t getFrameCount() const;
    uint32_t getRoutedPduCount() const;
    uint32_t getExtractedPduCount() const;
    uint32_t getDroppedFrameCount() const;

private:
    using FrameKey = std::tuple<uint8_t, uint8_t, uint32_t>;

    std::map<FrameKey, FrameConfig> m_frameTable;
    std::map<uint32_t, std::vector<GatewayDestination>> m_pduRoutes;
    std::map<uint32_t, std::vector<uint8_t>> m_pendingPdus;
    FrameTxSender m_txSender;
    PduSink m_pduSink;
    TpForwarder m_tpForwarder;
    PolicyEngine* m_policy = nullptr;
    uint16_t m_maxSingleFramePayload = 64U;
    uint32_t m_routedPduCount    = 0U;
    uint32_t m_extractedPduCount = 0U;
    uint32_t m_droppedFrameCount = 0U;
    bool m_initialized           = false;
};

} // namespace framegateway
