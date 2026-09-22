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
#include <vector>

namespace framegateway
{

/// One diagnostic session (physical or functional addressing).
struct DiagSession
{
    uint8_t channelType = 0U; // 0=CAN, 1=LIN, 2=ETH
    uint8_t channelId   = 0U;
    uint32_t requestFrameId  = 0U;
    uint32_t responseFrameId = 0U;
    /// Byte offset when multiplexed inside a multi-PDU frame.
    uint16_t pduOffset       = 0U;
    bool isActive            = true;
    uint32_t sessionTimeoutMs = 5000U;
    uint32_t lastActivityTimeMs = 0U;
    bool hasActivity         = false;
};

/// UDS service handler: (requestLength, request, responseOut, responseLengthInOut).
using UdsHandler = std::function<void(uint16_t, uint8_t const*, uint8_t*, uint16_t&)>;
/// Diagnostic frame transmit hook, same shape as FrameTxSender.
using DiagTxSender = std::function<bool(uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*)>;

/**
 * Diagnostic protocol link: routes extracted diagnostic PDUs to a UDS
 * handler and sends responses back.
 *
 * The UDS handler is injected (e.g. bound to OpenBSW's uds module), so this
 * module has no code dependency on it. Sessions are keyed by requestFrameId;
 * FrameGateway PDU ids carrying diagnostics must equal the requestFrameId.
 */
class DiagLink
{
public:
    DiagLink() = default;

    void init(std::vector<DiagSession> const& sessions);
    void shutdown();
    void clear();

    void onDiagnosticPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data,
                                 uint32_t nowMs);
    void sendDiagnosticResponse(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                uint16_t length, uint8_t const* data);

    void setUdsHandler(UdsHandler handler);
    void setTxSender(DiagTxSender sender);
    void setSessionActive(uint32_t requestFrameId, bool active, uint32_t nowMs);

    /// Session timeout handling; call every millisecond.
    void mainFunction(uint32_t nowMs);

    size_t getSessionCount() const;
    uint32_t getRequestCount() const;
    uint32_t getResponseCount() const;
    uint32_t getSessionTimeoutCount() const;

private:
    DiagSession* findSession(uint32_t requestFrameId);

    std::map<uint32_t, DiagSession> m_diagSessions;
    UdsHandler m_udsHandler;
    DiagTxSender m_txSender;
    uint32_t m_requestCount        = 0U;
    uint32_t m_responseCount       = 0U;
    uint32_t m_sessionTimeoutCount = 0U;
    bool m_initialized             = false;
};

} // namespace framegateway
