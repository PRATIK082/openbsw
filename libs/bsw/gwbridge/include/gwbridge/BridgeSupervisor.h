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
 * \ingroup gwbridge
 *
 * Reuse note: lifecycle integration mirrors commgateway::CommStack /
 * framegateway::GatewayStack (SimpleLifecycleComponent + async RunnableType
 * with a 1 ms scheduleAtFixedRate tick). Pumps the real routing::Router via
 * RouterBridge and drains channel TX queues via registered pump callbacks.
 */
#pragma once

#include "gwbridge/RouterBridge.h"

#include <async/Async.h>
#include <lifecycle/SimpleLifecycleComponent.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace gwbridge
{

/// Run period of the supervisor tick in milliseconds.
static uint32_t const BRIDGE_SUPERVISOR_PERIOD_MS = 1U;

/// Max routes pumped per tick (bounds Router::run() round-robin work).
static uint32_t const BRIDGE_MAX_ROUTES_PER_TICK = 32U;

/// One bridged channel: tables + raw endpoints + TX drain callback.
struct SupervisorChannel
{
    BridgeChannelConfig config;
    ::io::IReader* reader = nullptr;
    ::io::IWriter* writer = nullptr;
    std::function<uint32_t()> pumpTx;
};

/// Code-first supervisor configuration (max 8 channels).
struct SupervisorConfig
{
    std::vector<SupervisorChannel> channels;
    std::vector<BridgeRoute> routes;
};

/**
 * Top-level Phase 4 orchestrator: owns a RouterBridge<8> and pumps it plus
 * all channel TX queues from a 1 ms lifecycle tick.
 */
class BridgeSupervisor
: public ::lifecycle::SimpleLifecycleComponent
, private ::async::RunnableType
{
public:
    static BridgeSupervisor& getInstance();

    void configure(SupervisorConfig const& config, ::async::ContextType context);
    ::async::ContextType getTransitionContext(Transition::Type transition) override;

    RouterBridge<8U>& getRouterBridge();
    uint32_t getRoutedCount() const;
    uint32_t getTimeMs() const;
    bool isConfigured() const;
    /// Runs one pump tick (router + TX drains); test hook also usable by bare-metal loops.
    void pumpOnce();

protected:
    ~BridgeSupervisor() = default;

    void init() override;
    void run() override;
    void shutdown() override;

private:
    // ::async::RunnableType
    void execute() override;

    SupervisorConfig m_config{};
    ::async::ContextType m_context = ::async::CONTEXT_INVALID;
    ::async::TimeoutType m_timeout;
    bool m_configured = false;

    RouterBridge<8U> m_routerBridge;
    uint32_t m_routedCount = 0U;
    uint32_t m_timeMs      = 0U;
};

} // namespace gwbridge
