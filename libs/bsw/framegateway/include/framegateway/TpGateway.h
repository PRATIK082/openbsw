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
#include <map>
#include <tuple>
#include <vector>

namespace framegateway
{

/// Reassembled message sink: (channelType, channelId, frameId, length, data).
using TpMessageCallback = std::function<void(uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*)>;
/// Transport frame transmit hook, same shape as FrameTxSender.
using TpTxSender = std::function<bool(uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*)>;

/// ISO-TP (ISO 15765-2) timeout defaults in milliseconds.
static uint32_t const TP_N_AS_TIMEOUT_MS = 1000U;
static uint32_t const TP_N_BS_TIMEOUT_MS = 1000U;
static uint32_t const TP_N_CR_TIMEOUT_MS = 1000U;

/// One active segmentation/reassembly session (normal addressing, 11-bit id).
struct TpSession
{
    uint32_t sessionId      = 0U;
    uint8_t channelType     = 0U;
    uint8_t channelId       = 0U;
    uint32_t frameId        = 0U;
    uint32_t totalLength    = 0U;
    std::vector<uint8_t> buffer;
    uint32_t receivedLength    = 0U;
    uint32_t sentLength        = 0U;
    uint8_t nextSequenceNumber = 1U;
    uint16_t txFramePayload   = 8U;
    uint32_t lastActivityTimeMs = 0U;
    bool isTransmitting      = false;
    bool waitingForFlowControl = false;
    TpMessageCallback completionCallback;
};

/**
 * Transport protocol gateway (ISO-TP segmentation/reassembly).
 *
 * Covers SingleFrame (<=7 B, plus CAN FD escape up to 62 B), FirstFrame
 * (12-bit length), ConsecutiveFrames (4-bit sequence) and FlowControl
 * (CTS/WAIT/OVERFLOW). TX sessions pause for CTS; block sizes are accepted
 * but frames are sent back-to-back once CTS arrives (documented
 * simplification — STmin is parsed and stored but not delayed on).
 */
class TpGateway
{
public:
    TpGateway() = default;

    void init(uint32_t maxSessions, uint32_t maxPduSize);
    void shutdown();
    void clear();

    /**
     * Segments pduData and starts transmission.
     * \return session id (>0) or 0 when rejected (no sender, too big, table full).
     */
    uint32_t segmentAndTransmit(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                uint16_t pduLength, uint8_t const* pduData,
                                uint16_t maxFramePayload = 8U);

    void onTransportFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                  uint16_t length, uint8_t const* data, uint32_t nowMs);

    void setPduCallback(TpMessageCallback cb);
    void setTxSender(TpTxSender sender);

    /// Timeout handling (N_As/N_Bs/N_Cr); call every millisecond.
    void mainFunction(uint32_t nowMs);

    size_t getActiveSessionCount() const;
    uint32_t getSegmentationErrorCount() const;
    uint32_t getReassemblyTimeoutCount() const;

private:
    using SessionKey = std::tuple<uint8_t, uint8_t, uint32_t>;

    TpSession* findSession(uint8_t channelType, uint8_t channelId, uint32_t frameId);
    void handleSingleFrame(TpSession& session, uint8_t const* data, uint16_t length);
    void handleFirstFrame(TpSession& session, uint8_t const* data, uint16_t length,
                           uint16_t framePayload);
    void handleConsecutiveFrame(TpSession& session, uint8_t const* data, uint16_t length,
                                uint16_t framePayload);
    void handleFlowControl(TpSession& session, uint8_t const* data, uint16_t length,
                           uint16_t framePayload);
    void sendFrames(TpSession& session, uint16_t framePayload);
    void sendFlowControl(TpSession& session, uint8_t flowStatus);
    void abortSession(SessionKey const& key);

    std::map<SessionKey, TpSession> m_activeSessions;
    uint32_t m_maxSessions   = 0U;
    uint32_t m_maxPduSize    = 4095U;
    uint32_t m_nextSessionId = 1U;
    uint32_t m_currentTimeMs = 0U;
    TpMessageCallback m_pduCallback;
    TpTxSender m_txSender;
    uint32_t m_segmentationErrorCount  = 0U;
    uint32_t m_reassemblyTimeoutCount  = 0U;
    bool m_initialized                 = false;
};

} // namespace framegateway
