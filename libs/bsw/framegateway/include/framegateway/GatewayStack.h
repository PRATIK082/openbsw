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

#include "framegateway/DiagLink.h"
#include "framegateway/DoIpHandler.h"
#include "framegateway/FrameGateway.h"
#include "framegateway/GatewayPolicy.h"
#include "framegateway/TpGateway.h"
#include "framegateway/XcpServer.h"
#include "framegateway/XcpSymbolTable.h"

#include <async/Async.h>
#include <lifecycle/SimpleLifecycleComponent.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace framegateway
{

/// Run period of the gateway main functions in milliseconds.
static uint32_t const GATEWAYSTACK_RUN_PERIOD_MS = 1U;

/// Cross-layer gateway counters exposed for diagnostics/monitoring.
struct GatewayStatistics
{
    uint32_t framesReceived    = 0U;
    uint32_t framesTransmitted = 0U;
    uint32_t framesDropped     = 0U;
    uint32_t pdusExtracted     = 0U;
    uint32_t pdusRouted        = 0U;
    uint32_t pdusDropped       = 0U;
    uint32_t tpSessionsActive       = 0U;
    uint32_t tpSegmentationErrors   = 0U;
    uint32_t tpReassemblyTimeouts   = 0U;
    uint32_t diagRequestsReceived   = 0U;
    uint32_t diagResponsesSent      = 0U;
    uint32_t diagSessionTimeouts    = 0U;
    uint32_t xcpCommandsReceived    = 0U;
    uint32_t xcpDaqListsTriggered   = 0U;
    uint32_t xcpMemoryAccessErrors  = 0U;

    void reset();
};

/// Code-first configuration of the gateway stack.
struct GatewayStackConfig
{
    std::vector<FrameConfig> frameTable;
    std::vector<DiagSession> diagSessions;
    std::vector<XcpSymbol> xcpSymbols;
    std::vector<XcpDaqList> xcpDaqLists;
    std::vector<GatewayPolicy> policies;
    uint32_t tpMaxSessions = 4U;
    uint32_t tpMaxPduSize  = 4095U;
};

/**
 * Top-level Phase 3 orchestrator: FrameGateway + TpGateway + DiagLink +
 * DoIpHandler + XcpServer + PolicyEngine under one lifecycle component.
 *
 * Lower layers feed frames via onFrameReceived() (e.g. from a CanChannel
 * upper-layer callback — no Phase 1/2 code changes required). Outbound
 * traffic leaves through the injected FrameTxSender.
 */
class GatewayStack
: public ::lifecycle::SimpleLifecycleComponent
, private ::async::RunnableType
{
public:
    static GatewayStack& getInstance();

    void configure(GatewayStackConfig const& config, ::async::ContextType context);
    ::async::ContextType getTransitionContext(Transition::Type transition) override;

    /**
     * Applies a frame_gateway_config.json document (frameConfigs,
     * diagnosticSessions, xcpSymbols, gatewayPolicies).
     * \return false and fills error on schema violations.
     */
    bool loadConfig(std::string const& jsonText, std::string& error);

    /// Lower-layer entry point: (channelType, channelId, frameId, length, data).
    void onFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                         uint16_t length, uint8_t const* data);
    /// Lower-layer TP entry point (raw transport frames).
    void onTransportFrameReceived(uint8_t channelType, uint8_t channelId, uint32_t frameId,
                                  uint16_t length, uint8_t const* data);

    void setTxSender(FrameTxSender sender);
    void setUdsHandler(UdsHandler handler);

    FrameGateway& getFrameGateway();
    TpGateway& getTpGateway();
    DiagLink& getDiagLink();
    DoIpHandler& getDoIpHandler();
    XcpServer& getXcpServer();
    PolicyEngine& getPolicyEngine();
    GatewayStatistics getStatistics() const;
    void resetStatistics();
    void publishMetrics();
    uint32_t getTimeMs() const;
    bool isConfigured() const;

protected:
    ~GatewayStack() = default;

    void init() override;
    void run() override;
    void shutdown() override;

private:
    // ::async::RunnableType
    void execute() override;

    GatewayStackConfig m_config{};
    ::async::ContextType m_context = ::async::CONTEXT_INVALID;
    ::async::TimeoutType m_timeout;
    bool m_configured = false;

    FrameGateway m_frameGateway;
    TpGateway m_tpGateway;
    DiagLink m_diagLink;
    DoIpHandler m_doIpHandler;
    XcpServer m_xcpServer;
    PolicyEngine m_policyEngine;
    GatewayStatistics m_stats;
    FrameTxSender m_txSender;
    uint32_t m_timeMs = 0U;
};

} // namespace framegateway
