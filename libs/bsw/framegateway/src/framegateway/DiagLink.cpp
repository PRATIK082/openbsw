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
#include "framegateway/DiagLink.h"

namespace framegateway
{

void DiagLink::init(std::vector<DiagSession> const& sessions)
{
    clear();
    for (auto const& session : sessions)
    {
        m_diagSessions[session.requestFrameId] = session;
    }
    m_initialized = true;
}

void DiagLink::shutdown()
{
    m_initialized = false;
    clear();
}

void DiagLink::clear()
{
    m_diagSessions.clear();
    m_requestCount         = 0U;
    m_responseCount        = 0U;
    m_sessionTimeoutCount  = 0U;
}

void DiagLink::onDiagnosticPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data,
                                       uint32_t nowMs)
{
    if (!m_initialized || (data == nullptr) || (length == 0U))
    {
        return;
    }
    DiagSession* session = findSession(pduId);
    if ((session == nullptr) || !session->isActive)
    {
        return;
    }
    if ((session->sessionTimeoutMs > 0U) && session->hasActivity
        && ((nowMs - session->lastActivityTimeMs) >= session->sessionTimeoutMs))
    {
        session->isActive = false;
        m_sessionTimeoutCount++;
        return;
    }
    session->lastActivityTimeMs = nowMs;
    session->hasActivity        = true;
    m_requestCount++;
    if (!m_udsHandler || !m_txSender)
    {
        return;
    }
    uint8_t response[4096U] = {0U};
    uint16_t responseLength  = sizeof(response);
    m_udsHandler(length, data, response, responseLength);
    if (responseLength > 0U)
    {
        sendDiagnosticResponse(session->channelType, session->channelId, session->responseFrameId,
                               responseLength, response);
    }
}

void DiagLink::sendDiagnosticResponse(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                      uint16_t length, uint8_t const* data)
{
    if (!m_initialized || !m_txSender || (data == nullptr))
    {
        return;
    }
    if (m_txSender(channelType, channelId, frameId, length, data))
    {
        m_responseCount++;
    }
}

void DiagLink::setUdsHandler(UdsHandler handler) { m_udsHandler = handler; }

void DiagLink::setTxSender(DiagTxSender sender) { m_txSender = sender; }

void DiagLink::setSessionActive(uint32_t requestFrameId, bool active, uint32_t nowMs)
{
    DiagSession* session = findSession(requestFrameId);
    if (session != nullptr)
    {
        session->isActive           = active;
        session->lastActivityTimeMs = nowMs;
    }
}

void DiagLink::mainFunction(uint32_t nowMs)
{
    if (!m_initialized)
    {
        return;
    }
    for (auto& entry : m_diagSessions)
    {
        DiagSession& session = entry.second;
        if (session.isActive && (session.sessionTimeoutMs > 0U) && session.hasActivity
            && ((nowMs - session.lastActivityTimeMs) >= session.sessionTimeoutMs))
        {
            session.isActive = false;
            m_sessionTimeoutCount++;
        }
    }
}

size_t DiagLink::getSessionCount() const { return m_diagSessions.size(); }

uint32_t DiagLink::getRequestCount() const { return m_requestCount; }

uint32_t DiagLink::getResponseCount() const { return m_responseCount; }

uint32_t DiagLink::getSessionTimeoutCount() const { return m_sessionTimeoutCount; }

DiagSession* DiagLink::findSession(uint32_t requestFrameId)
{
    auto it = m_diagSessions.find(requestFrameId);
    return (it != m_diagSessions.end()) ? &it->second : nullptr;
}

} // namespace framegateway
