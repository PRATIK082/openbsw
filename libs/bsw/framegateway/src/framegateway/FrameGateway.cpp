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
#include "framegateway/FrameGateway.h"

namespace framegateway
{

void FrameGateway::init(std::vector<FrameConfig> const& frameTable)
{
    clear();
    for (auto const& frame : frameTable)
    {
        m_frameTable[FrameKey(frame.channelType, frame.channelId, frame.frameId)] = frame;
    }
    m_initialized = true;
}

void FrameGateway::shutdown()
{
    m_initialized = false;
    clear();
}

void FrameGateway::clear()
{
    m_frameTable.clear();
    m_pduRoutes.clear();
    m_pendingPdus.clear();
    m_routedPduCount    = 0U;
    m_extractedPduCount = 0U;
    m_droppedFrameCount = 0U;
}

void FrameGateway::onFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                   uint16_t length, uint8_t const* data)
{
    if (!m_initialized || (data == nullptr))
    {
        return;
    }
    // Policy check works on a mutable copy (TRANSFORM may edit in place).
    std::vector<uint8_t> frameBuffer(data, data + length);
    uint8_t policyChannelType = channelType;
    uint8_t policyChannelId   = channelId;
    uint32_t policyFrameId    = frameId;
    uint16_t policyLength     = length;
    if (m_policy != nullptr)
    {
        // PolicyEngine has no time base here; rate windows use mainFunction-
        // driven resets only when evaluatePolicy is called with wall time.
        // Pass 0 and rely on mainFunction() resets for window rollover.
        PolicyAction const verdict = m_policy->evaluatePolicy(
            policyChannelType, policyChannelId, policyFrameId, frameBuffer.data(), policyLength,
            0U);
        if (verdict == PolicyAction::DENY)
        {
            m_droppedFrameCount++;
            return;
        }
    }
    FrameConfig const* frame = findFrame(policyChannelType, policyChannelId, policyFrameId);
    if (frame == nullptr)
    {
        m_droppedFrameCount++;
        return;
    }
    if (policyLength > frame->frameLength)
    {
        m_droppedFrameCount++;
        return;
    }
    extractPdus(*frame, frameBuffer.data(), policyLength);
}

void FrameGateway::extractPdus(FrameConfig const& frame, uint8_t const* frameData, uint16_t length)
{
    if (frameData == nullptr)
    {
        return;
    }
    PduDataMap const pdus = PduAssembler::unpackMultiPdu(frameData, length, frame.pdus);
    for (auto const& entry : frame.pdus)
    {
        auto it = pdus.find(entry.pduId);
        if (it == pdus.end())
        {
            continue; // out-of-range layout entry
        }
        uint16_t pduLength   = it->second.first;
        uint8_t const* pduData = it->second.second;
        if (entry.isVariableLength)
        {
            pduLength = PduAssembler::extractVariableLengthPdu(
                pduData, pduLength, entry.lengthFieldOffset, entry.length);
        }
        bool const consume = (frame.action == GatewayAction::EXTRACT_AND_SIGNAL)
                             || (frame.action == GatewayAction::BOTH);
        bool const forward = (frame.action == GatewayAction::ROUTE_ONLY)
                             || (frame.action == GatewayAction::BOTH);
        if (consume)
        {
            m_extractedPduCount++;
            if (m_pduSink)
            {
                m_pduSink(entry.pduId, pduLength, pduData);
            }
        }
        if (forward)
        {
            if ((m_tpForwarder) && (pduLength > m_maxSingleFramePayload))
            {
                m_tpForwarder(frame.channelType, frame.channelId, frame.frameId, pduLength,
                              pduData);
            }
            else
            {
                routePdu(entry.pduId, pduLength, pduData);
            }
        }
    }
}

void FrameGateway::routePdu(uint32_t pduId, uint16_t length, uint8_t const* data)
{
    auto it = m_pduRoutes.find(pduId);
    if ((it == m_pduRoutes.end()) || !m_txSender)
    {
        return;
    }
    for (auto const& destination : it->second)
    {
        if (m_txSender(destination.channelType, destination.channelId, destination.frameId,
                       length, data))
        {
            m_routedPduCount++;
        }
    }
}

void FrameGateway::registerPduRoute(uint32_t pduId, std::vector<GatewayDestination> const& destinations)
{
    m_pduRoutes[pduId] = destinations;
}

bool FrameGateway::storePdu(uint32_t pduId, uint16_t length, uint8_t const* data)
{
    if (data == nullptr)
    {
        return false;
    }
    m_pendingPdus[pduId] = std::vector<uint8_t>(data, data + length);
    return true;
}

bool FrameGateway::packAndTransmit(uint8_t channelType, uint8_t channelId, uint32_t frameId)
{
    FrameConfig const* frame = findFrame(channelType, channelId, frameId);
    if ((frame == nullptr) || !m_txSender)
    {
        return false;
    }
    std::vector<uint8_t> txBuffer(frame->frameLength, 0xFFU);
    PduDataMap pduData;
    for (auto const& entry : frame->pdus)
    {
        auto it = m_pendingPdus.find(entry.pduId);
        if (it == m_pendingPdus.end())
        {
            continue;
        }
        uint16_t const copyLength
            = (it->second.size() > entry.length) ? entry.length
                                                 : static_cast<uint16_t>(it->second.size());
        pduData[entry.pduId] = std::make_pair(copyLength, it->second.data());
    }
    if (!PduAssembler::packMultiPdu(txBuffer.data(), frame->frameLength, frame->pdus, pduData))
    {
        return false;
    }
    return m_txSender(channelType, channelId, frameId, frame->frameLength, txBuffer.data());
}

void FrameGateway::setTxSender(FrameTxSender sender) { m_txSender = sender; }

void FrameGateway::setPduSink(PduSink sink) { m_pduSink = sink; }

void FrameGateway::setTpForwarder(TpForwarder forwarder) { m_tpForwarder = forwarder; }

void FrameGateway::setPolicyEngine(PolicyEngine* policy) { m_policy = policy; }

void FrameGateway::setMaxSingleFramePayload(uint16_t payload) { m_maxSingleFramePayload = payload; }

FrameConfig const* FrameGateway::findFrame(uint8_t channelType, uint8_t channelId,
                                           uint32_t frameId) const
{
    auto it = m_frameTable.find(FrameKey(channelType, channelId, frameId));
    return (it != m_frameTable.end()) ? &it->second : nullptr;
}

size_t FrameGateway::getFrameCount() const { return m_frameTable.size(); }

uint32_t FrameGateway::getRoutedPduCount() const { return m_routedPduCount; }

uint32_t FrameGateway::getExtractedPduCount() const { return m_extractedPduCount; }

uint32_t FrameGateway::getDroppedFrameCount() const { return m_droppedFrameCount; }

} // namespace framegateway
