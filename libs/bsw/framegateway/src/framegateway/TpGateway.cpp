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
#include "framegateway/TpGateway.h"

#include <cstring>

namespace framegateway
{
namespace
{

uint8_t const PCI_TYPE_MASK     = 0xF0U;
uint8_t const PCI_SF            = 0x00U;
uint8_t const PCI_FF            = 0x10U;
uint8_t const PCI_CF            = 0x20U;
uint8_t const PCI_FC            = 0x30U;
uint8_t const FC_STATUS_CTS     = 0x00U;
uint8_t const FC_STATUS_WAIT    = 0x01U;
uint8_t const FC_STATUS_OVERFLOW = 0x02U;

} // namespace

void TpGateway::init(uint32_t maxSessions, uint32_t maxPduSize)
{
    clear();
    m_maxSessions  = maxSessions;
    m_maxPduSize   = maxPduSize;
    m_initialized  = true;
}

void TpGateway::shutdown()
{
    m_initialized = false;
    clear();
}

void TpGateway::clear()
{
    m_activeSessions.clear();
    m_segmentationErrorCount = 0U;
    m_reassemblyTimeoutCount = 0U;
}

uint32_t TpGateway::segmentAndTransmit(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                       uint16_t pduLength, uint8_t const* pduData,
                                       uint16_t maxFramePayload)
{
    if (!m_initialized || !m_txSender || (pduData == nullptr) || (pduLength == 0U)
        || (pduLength > m_maxPduSize) || (maxFramePayload < 8U))
    {
        m_segmentationErrorCount++;
        return 0U;
    }
    SessionKey const key(channelType, channelId, frameId);
    if ((m_activeSessions.find(key) != m_activeSessions.end())
        || (m_activeSessions.size() >= m_maxSessions))
    {
        m_segmentationErrorCount++;
        return 0U;
    }

    TpSession session{};
    session.sessionId       = m_nextSessionId++;
    session.channelType     = channelType;
    session.channelId       = channelId;
    session.frameId         = frameId;
    session.totalLength     = pduLength;
    session.buffer.assign(pduData, pduData + pduLength);
    session.isTransmitting  = true;
    session.completionCallback = m_pduCallback;

    // Single frame fits: classic (<=7 B) or CAN FD escape (<= payload - 2).
    bool const useEscape = (maxFramePayload > 8U) && (pduLength > 7U)
                           && (pduLength <= (maxFramePayload - 2U));
    if ((pduLength <= 7U) || useEscape)
    {
        std::vector<uint8_t> frame;
        if (!useEscape)
        {
            frame.push_back(static_cast<uint8_t>(pduLength & 0x0FU));
        }
        else
        {
            frame.push_back(0x00U);
            frame.push_back(static_cast<uint8_t>(pduLength));
        }
        frame.insert(frame.end(), pduData, pduData + pduLength);
        if (!m_txSender(channelType, channelId, frameId, static_cast<uint16_t>(frame.size()),
                        frame.data()))
        {
            m_segmentationErrorCount++;
            return 0U;
        }
        return session.sessionId;
    }

    // Multi-frame: send FirstFrame, wait for FlowControl.
    // Note: the session is stored BEFORE transmitting because a synchronous
    // transport may re-enter with the FlowControl frame during the send call.
    uint16_t const ffDataBytes = (maxFramePayload - 2U < 6U) ? (maxFramePayload - 2U) : 6U;
    std::vector<uint8_t> ff;
    ff.push_back(static_cast<uint8_t>(PCI_FF | ((pduLength >> 8U) & 0x0FU)));
    ff.push_back(static_cast<uint8_t>(pduLength & 0xFFU));
    ff.insert(ff.end(), pduData, pduData + ffDataBytes);
    session.sentLength            = ffDataBytes;
    session.waitingForFlowControl = true;
    session.lastActivityTimeMs    = m_currentTimeMs;
    session.txFramePayload        = maxFramePayload;
    uint32_t const sessionId      = session.sessionId;
    m_activeSessions[key]         = session;
    if (!m_txSender(channelType, channelId, frameId, static_cast<uint16_t>(ff.size()), ff.data()))
    {
        m_activeSessions.erase(key);
        m_segmentationErrorCount++;
        return 0U;
    }
    // The session may already be gone (synchronous transport completed
    // the transfer re-entrantly during the send call above).
    return sessionId;
}

void TpGateway::onTransportFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                         uint16_t length, uint8_t const* data, uint32_t nowMs)
{
    if (!m_initialized || (data == nullptr) || (length == 0U))
    {
        return;
    }
    SessionKey const key(channelType, channelId, frameId);
    uint8_t const pciType = data[0] & PCI_TYPE_MASK;

    if (pciType == PCI_FC)
    {
        TpSession* session = findSession(channelType, channelId, frameId);
        if (session != nullptr)
        {
            handleFlowControl(*session, data, length, length);
            session->lastActivityTimeMs = nowMs;
        }
        return;
    }

    if (pciType == PCI_CF)
    {
        TpSession* session = findSession(channelType, channelId, frameId);
        if ((session == nullptr) || session->isTransmitting)
        {
            m_segmentationErrorCount++;
            return;
        }
        handleConsecutiveFrame(*session, data, length, length);
        session->lastActivityTimeMs = nowMs;
        if (session->receivedLength >= session->totalLength)
        {
            TpSession done = *session;
            m_activeSessions.erase(key);
            if (done.completionCallback)
            {
                done.completionCallback(done.channelType, done.channelId, done.frameId,
                                        done.totalLength, done.buffer.data());
            }
            else if (m_pduCallback)
            {
                m_pduCallback(done.channelType, done.channelId, done.frameId, done.totalLength,
                              done.buffer.data());
            }
        }
        return;
    }

    // SF or FF start a new reception (any stale session is replaced).
    if ((m_activeSessions.size() >= m_maxSessions)
        && (m_activeSessions.find(key) == m_activeSessions.end()))
    {
        m_segmentationErrorCount++;
        return;
    }
    TpSession session{};
    session.sessionId       = m_nextSessionId++;
    session.channelType     = channelType;
    session.channelId       = channelId;
    session.frameId         = frameId;
    session.lastActivityTimeMs = nowMs;
    session.completionCallback = m_pduCallback;

    if (pciType == PCI_SF)
    {
        handleSingleFrame(session, data, length);
        if (session.totalLength > 0U)
        {
            if (session.completionCallback)
            {
                session.completionCallback(channelType, channelId, frameId, session.totalLength,
                                           session.buffer.data());
            }
            else if (m_pduCallback)
            {
                m_pduCallback(channelType, channelId, frameId, session.totalLength,
                              session.buffer.data());
            }
        }
        return;
    }
    if (pciType == PCI_FF)
    {
        handleFirstFrame(session, data, length, length);
        if (session.totalLength == 0U)
        {
            return; // rejected (too big / malformed)
        }
        m_activeSessions[key] = session;
        sendFlowControl(m_activeSessions[key], FC_STATUS_CTS);
        return;
    }
    m_segmentationErrorCount++;
}

void TpGateway::setPduCallback(TpMessageCallback cb) { m_pduCallback = cb; }

void TpGateway::setTxSender(TpTxSender sender) { m_txSender = sender; }

void TpGateway::mainFunction(uint32_t nowMs)
{
    if (!m_initialized)
    {
        return;
    }
    m_currentTimeMs = nowMs;
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end();)
    {
        TpSession& session = it->second;
        uint32_t const timeout
            = session.waitingForFlowControl ? TP_N_BS_TIMEOUT_MS : TP_N_CR_TIMEOUT_MS;
        if (((nowMs - session.lastActivityTimeMs) >= timeout) && (timeout > 0U))
        {
            if (session.waitingForFlowControl)
            {
                m_segmentationErrorCount++;
            }
            else
            {
                m_reassemblyTimeoutCount++;
            }
            it = m_activeSessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

size_t TpGateway::getActiveSessionCount() const { return m_activeSessions.size(); }

uint32_t TpGateway::getSegmentationErrorCount() const { return m_segmentationErrorCount; }

uint32_t TpGateway::getReassemblyTimeoutCount() const { return m_reassemblyTimeoutCount; }

TpSession* TpGateway::findSession(uint8_t channelType, uint8_t channelId, uint32_t frameId)
{
    auto it = m_activeSessions.find(SessionKey(channelType, channelId, frameId));
    return (it != m_activeSessions.end()) ? &it->second : nullptr;
}

void TpGateway::handleSingleFrame(TpSession& session, uint8_t const* data, uint16_t length)
{
    uint32_t sfLength = data[0] & 0x0FU;
    uint16_t headerBytes = 1U;
    if ((data[0] == 0x00U) && (length >= 2U))
    {
        // CAN FD escape: second byte carries the length.
        sfLength    = data[1];
        headerBytes = 2U;
    }
    if ((sfLength > m_maxPduSize) || ((sfLength + headerBytes) > length))
    {
        session.totalLength = 0U;
        m_segmentationErrorCount++;
        return;
    }
    session.totalLength    = sfLength;
    session.receivedLength = sfLength;
    session.buffer.assign(data + headerBytes, data + headerBytes + sfLength);
}

void TpGateway::handleFirstFrame(TpSession& session, uint8_t const* data, uint16_t length,
                                 uint16_t framePayload)
{
    if (length < 2U)
    {
        m_segmentationErrorCount++;
        return;
    }
    uint32_t const totalLength
        = (static_cast<uint32_t>(data[0] & 0x0FU) << 8U) | static_cast<uint32_t>(data[1]);
    if ((totalLength == 0U) || (totalLength > m_maxPduSize))
    {
        session.totalLength = 0U;
        m_segmentationErrorCount++;
        return;
    }
    session.totalLength = totalLength;
    session.buffer.resize(totalLength);
    uint16_t const firstBytes
        = ((framePayload - 2U) < totalLength) ? (framePayload - 2U)
                                             : static_cast<uint16_t>(totalLength);
    (void)std::memcpy(session.buffer.data(), data + 2U, firstBytes);
    session.receivedLength     = firstBytes;
    session.nextSequenceNumber = 1U;
}

void TpGateway::handleConsecutiveFrame(TpSession& session, uint8_t const* data, uint16_t length,
                                       uint16_t framePayload)
{
    uint8_t const sequence = data[0] & 0x0FU;
    if (sequence != session.nextSequenceNumber)
    {
        abortSession(SessionKey(session.channelType, session.channelId, session.frameId));
        m_segmentationErrorCount++;
        return;
    }
    uint32_t const remaining = session.totalLength - session.receivedLength;
    uint16_t const available = (length > 1U) ? (length - 1U) : 0U;
    (void)framePayload;
    uint16_t const chunk = (remaining < available) ? static_cast<uint16_t>(remaining) : available;
    (void)std::memcpy(session.buffer.data() + session.receivedLength, data + 1U, chunk);
    session.receivedLength += chunk;
    session.nextSequenceNumber = static_cast<uint8_t>((session.nextSequenceNumber + 1U) & 0x0FU);
}

void TpGateway::handleFlowControl(TpSession& session, uint8_t const* data, uint16_t length,
                                  uint16_t framePayload)
{
    (void)framePayload;
    if (!session.isTransmitting || !session.waitingForFlowControl || (length < 3U))
    {
        return;
    }
    uint8_t const flowStatus = data[0] & 0x0FU;
    if (flowStatus == FC_STATUS_CTS)
    {
        session.waitingForFlowControl = false;
        sendFrames(session, session.txFramePayload);
        if (session.sentLength >= session.totalLength)
        {
            abortSession(SessionKey(session.channelType, session.channelId, session.frameId));
        }
    }
    else if (flowStatus == FC_STATUS_OVERFLOW)
    {
        abortSession(SessionKey(session.channelType, session.channelId, session.frameId));
        m_segmentationErrorCount++;
    }
    // WAIT: keep waiting, timer re-armed by caller.
}

void TpGateway::sendFrames(TpSession& session, uint16_t framePayload)
{
    uint16_t const dataPerFrame = (framePayload > 1U) ? (framePayload - 1U) : 7U;
    std::vector<uint8_t> frame;
    frame.resize(framePayload);
    while (session.sentLength < session.totalLength)
    {
        uint32_t const remaining = session.totalLength - session.sentLength;
        uint16_t const chunk
            = (remaining < dataPerFrame) ? static_cast<uint16_t>(remaining) : dataPerFrame;
        frame[0] = static_cast<uint8_t>(PCI_CF | session.nextSequenceNumber);
        (void)std::memcpy(frame.data() + 1U, session.buffer.data() + session.sentLength, chunk);
        if (!m_txSender(session.channelType, session.channelId, session.frameId,
                        static_cast<uint16_t>(chunk + 1U), frame.data()))
        {
            m_segmentationErrorCount++;
            return;
        }
        session.sentLength += chunk;
        session.nextSequenceNumber
            = static_cast<uint8_t>((session.nextSequenceNumber + 1U) & 0x0FU);
    }
}

void TpGateway::sendFlowControl(TpSession& session, uint8_t flowStatus)
{
    if (!m_txSender)
    {
        return;
    }
    uint8_t const fc[3U] = {static_cast<uint8_t>(PCI_FC | flowStatus), 0x00U, 0x00U};
    (void)m_txSender(session.channelType, session.channelId, session.frameId, 3U, fc);
}

void TpGateway::abortSession(SessionKey const& key) { m_activeSessions.erase(key); }

} // namespace framegateway
