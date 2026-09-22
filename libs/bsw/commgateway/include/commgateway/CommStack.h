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
 * \ingroup commgateway
 */
#pragma once

#include "commgateway/CommStateManager.h"
#include "commgateway/CommStatistics.h"
#include "commgateway/EthIpdu.h"
#include "commgateway/LinChannel.h"
#include "commgateway/SignalGateway.h"
#include "commgateway/TimeoutMonitor.h"
#include "commgateway/TxConfirmationMgr.h"

#include "canstack/CanInterface.h"

#include <async/Async.h>
#include <lifecycle/SimpleLifecycleComponent.h>

#include <cstdint>
#include <map>
#include <string>

namespace commgateway
{

/// Run period of the gateway main functions in milliseconds.
static uint32_t const COMMSTACK_RUN_PERIOD_MS = 1U;

/// Code-first configuration of the gateway stack.
struct CommStackConfig
{
    /// Existing CAN stack config; nullptr disables the CAN path.
    ::canstack::CanStackConfig const* canConfig = nullptr;
    LinHwInterface* linHw       = nullptr;
    uint8_t linChannelId        = 0U;
    uint32_t linBaudrate        = 19200U;
    EthTransportIf* ethTransport = nullptr;
    EthIpduConfig ethConfig{};
};

/**
 * Top-level orchestrator over CAN (existing), LIN and Ethernet.
 *
 * Wires the protocol channels into SignalGateway/TimeoutMonitor/
 * CommStateManager and drives every mainFunction() from a 1 ms async
 * tick. The existing canstack modules are used through their public API
 * only (no modifications).
 */
class CommStack
: public ::lifecycle::SimpleLifecycleComponent
, private ::async::RunnableType
{
public:
    static CommStack& getInstance();

    void configure(CommStackConfig const& config, ::async::ContextType context);
    ::async::ContextType getTransitionContext(Transition::Type transition) override;

    /**
     * Loads gateway_rules.json content (signalRouting, frameTimeouts,
     * channelStates); \return false and fills error on schema violations.
     */
    bool loadGatewayRules(std::string const& jsonText, std::string& error);
    /// Maps an Ethernet PDU id to a signal name for rx demultiplexing.
    void registerEthSignalMapping(uint32_t pduId, std::string const& signalName);

    SignalGateway& getSignalGateway();
    TimeoutMonitor& getTimeoutMonitor();
    CommStateManager& getCommStateManager();
    LinChannel& getLinChannel();
    EthIpduManager& getEthIpduManager();
    ::canstack::CanInterface& getCanInterface();
    CommStatistics getStatistics() const;
    uint32_t getTimeMs() const;
    bool isConfigured() const;

protected:
    ~CommStack() = default;

    void init() override;
    void run() override;
    void shutdown() override;

private:
    // ::async::RunnableType
    void execute() override;

    bool dispatchToChannel(uint8_t channelType, uint32_t channelId, std::string const& signal,
                           double value);
    void onLinFrameReceived(uint8_t channelId, uint8_t pid, uint8_t* data, uint8_t dlc);
    void onEthPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data);

    CommStackConfig m_config{};
    ::async::ContextType m_context = ::async::CONTEXT_INVALID;
    ::async::TimeoutType m_timeout;
    bool m_configured = false;

    ::canstack::CanInterface m_canInterface;
    LinChannel m_linChannel;
    EthIpduManager m_ethIpdu;
    SignalGateway m_signalGateway;
    TimeoutMonitor m_timeoutMonitor;
    CommStateManager m_commStateManager;
    CommStatistics m_stats;
    uint32_t m_timeMs = 0U;
    std::map<uint32_t, std::string> m_ethSignalMap;
};

} // namespace commgateway
