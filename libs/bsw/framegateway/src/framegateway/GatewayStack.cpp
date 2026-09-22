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
#include "framegateway/GatewayStack.h"

#include "framegateway/GatewayConfigParser.h"

namespace framegateway
{

void GatewayStatistics::reset()
{
    framesReceived       = 0U;
    framesTransmitted    = 0U;
    framesDropped        = 0U;
    pdusExtracted        = 0U;
    pdusRouted           = 0U;
    pdusDropped          = 0U;
    tpSessionsActive     = 0U;
    tpSegmentationErrors = 0U;
    tpReassemblyTimeouts = 0U;
    diagRequestsReceived = 0U;
    diagResponsesSent    = 0U;
    diagSessionTimeouts  = 0U;
    xcpCommandsReceived  = 0U;
    xcpDaqListsTriggered = 0U;
    xcpMemoryAccessErrors = 0U;
}

GatewayStack& GatewayStack::getInstance()
{
    static GatewayStack instance;
    return instance;
}

void GatewayStack::configure(GatewayStackConfig const& config, ::async::ContextType context)
{
    m_config     = config;
    m_context    = context;
    m_configured = true;
}

::async::ContextType GatewayStack::getTransitionContext(Transition::Type const /* transition */)
{
    return m_context;
}

bool GatewayStack::loadConfig(std::string const& jsonText, std::string& error)
{
    return GatewayConfigParser::parse(jsonText, m_frameGateway, m_diagLink, m_xcpServer,
                                      m_policyEngine, error);
}

void GatewayStack::onFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                   uint16_t length, uint8_t const* data)
{
    m_stats.framesReceived++;
    uint32_t const routedBefore    = m_frameGateway.getRoutedPduCount();
    uint32_t const extractedBefore = m_frameGateway.getExtractedPduCount();
    uint32_t const droppedBefore   = m_frameGateway.getDroppedFrameCount();
    m_frameGateway.onFrameReceived(channelType, channelId, frameId, length, data);
    m_stats.pdusRouted += m_frameGateway.getRoutedPduCount() - routedBefore;
    m_stats.pdusExtracted += m_frameGateway.getExtractedPduCount() - extractedBefore;
    m_stats.framesDropped += m_frameGateway.getDroppedFrameCount() - droppedBefore;
}

void GatewayStack::onTransportFrameReceived(uint8_t channelType, uint8_t channelId,
                                            uint32_t frameId, uint16_t length,
                                            uint8_t const* data)
{
    m_stats.framesReceived++;
    m_tpGateway.onTransportFrameReceived(channelType, channelId, frameId, length, data, m_timeMs);
    m_stats.tpSessionsActive = static_cast<uint32_t>(m_tpGateway.getActiveSessionCount());
}

void GatewayStack::setTxSender(FrameTxSender sender)
{
    m_txSender = sender;
    m_frameGateway.setTxSender(sender);
    m_tpGateway.setTxSender(sender);
    m_diagLink.setTxSender(sender);
    m_xcpServer.setTransportHandler(
        [sender](uint8_t channelType, uint8_t channelId, uint32_t frameId, uint16_t length,
                 uint8_t const* data) {
            (void)sender(channelType, channelId, frameId, length, data);
        });
}

void GatewayStack::setUdsHandler(UdsHandler handler) { m_diagLink.setUdsHandler(handler); }

FrameGateway& GatewayStack::getFrameGateway() { return m_frameGateway; }

TpGateway& GatewayStack::getTpGateway() { return m_tpGateway; }

DiagLink& GatewayStack::getDiagLink() { return m_diagLink; }

DoIpHandler& GatewayStack::getDoIpHandler() { return m_doIpHandler; }

XcpServer& GatewayStack::getXcpServer() { return m_xcpServer; }

PolicyEngine& GatewayStack::getPolicyEngine() { return m_policyEngine; }

GatewayStatistics GatewayStack::getStatistics() const { return m_stats; }

void GatewayStack::resetStatistics() { m_stats.reset(); }

void GatewayStack::publishMetrics()
{
    // Hook for OpenBSW monitoring integration (e.g. logger/middleware publish).
}

uint32_t GatewayStack::getTimeMs() const { return m_timeMs; }

bool GatewayStack::isConfigured() const { return m_configured; }

void GatewayStack::init()
{
    if (!m_configured)
    {
        transitionDone();
        return;
    }
    m_frameGateway.init(m_config.frameTable);
    m_frameGateway.setPolicyEngine(&m_policyEngine);
    m_frameGateway.setTpForwarder([this](uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                         uint16_t length, uint8_t const* data) {
        m_tpGateway.onTransportFrameReceived(channelType, channelId, frameId, length, data,
                                             m_timeMs);
    });
    for (auto const& policy : m_config.policies)
    {
        m_policyEngine.addPolicy(policy);
    }
    m_tpGateway.init(m_config.tpMaxSessions, m_config.tpMaxPduSize);
    m_diagLink.init(m_config.diagSessions);
    m_xcpServer.init(m_config.xcpSymbols, m_config.xcpDaqLists);
    if (m_txSender)
    {
        setTxSender(m_txSender);
    }
    transitionDone();
}

void GatewayStack::run()
{
    ::async::scheduleAtFixedRate(m_context, *this, m_timeout, GATEWAYSTACK_RUN_PERIOD_MS,
                                 ::async::TimeUnit::MILLISECONDS);
    transitionDone();
}

void GatewayStack::shutdown()
{
    m_timeout.cancel();
    m_frameGateway.shutdown();
    m_tpGateway.shutdown();
    m_diagLink.shutdown();
    m_doIpHandler.shutdown();
    m_xcpServer.shutdown();
    transitionDone();
}

void GatewayStack::execute()
{
    m_timeMs += GATEWAYSTACK_RUN_PERIOD_MS;
    m_tpGateway.mainFunction(m_timeMs);
    m_diagLink.mainFunction(m_timeMs);
    m_xcpServer.mainFunction(m_timeMs);
    m_policyEngine.mainFunction(m_timeMs);

    m_stats.tpSessionsActive     = static_cast<uint32_t>(m_tpGateway.getActiveSessionCount());
    m_stats.tpSegmentationErrors = m_tpGateway.getSegmentationErrorCount();
    m_stats.tpReassemblyTimeouts = m_tpGateway.getReassemblyTimeoutCount();
    m_stats.diagRequestsReceived = m_diagLink.getRequestCount();
    m_stats.diagResponsesSent    = m_diagLink.getResponseCount();
    m_stats.diagSessionTimeouts  = m_diagLink.getSessionTimeoutCount();
    m_stats.xcpCommandsReceived  = m_xcpServer.getCommandCount();
    m_stats.xcpDaqListsTriggered = m_xcpServer.getDaqTriggerCount();
    m_stats.xcpMemoryAccessErrors = m_xcpServer.getMemoryErrorCount();
}

} // namespace framegateway
