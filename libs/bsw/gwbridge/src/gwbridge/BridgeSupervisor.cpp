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
 */
#include "gwbridge/BridgeSupervisor.h"

namespace gwbridge
{

BridgeSupervisor& BridgeSupervisor::getInstance()
{
    static BridgeSupervisor instance;
    return instance;
}

void BridgeSupervisor::configure(SupervisorConfig const& config, ::async::ContextType context)
{
    m_config     = config;
    m_context    = context;
    m_configured = true;
}

::async::ContextType BridgeSupervisor::getTransitionContext(Transition::Type const /* transition */)
{
    return m_context;
}

RouterBridge<8U>& BridgeSupervisor::getRouterBridge() { return m_routerBridge; }

uint32_t BridgeSupervisor::getRoutedCount() const { return m_routedCount; }

uint32_t BridgeSupervisor::getTimeMs() const { return m_timeMs; }

bool BridgeSupervisor::isConfigured() const { return m_configured; }

void BridgeSupervisor::pumpOnce() { execute(); }

void BridgeSupervisor::init()
{
    if (!m_configured)
    {
        transitionDone();
        return;
    }
    m_routerBridge.clear();
    for (auto const& channel : m_config.channels)
    {
        (void)m_routerBridge.addChannel(channel.config);
    }
    for (auto const& route : m_config.routes)
    {
        (void)m_routerBridge.addRoute(route);
    }
    ::io::IReader* readers[8U] = {};
    ::io::IWriter* writers[8U] = {};
    for (auto const& channel : m_config.channels)
    {
        if (channel.config.channelIndex < 8U)
        {
            readers[channel.config.channelIndex] = channel.reader;
            writers[channel.config.channelIndex] = channel.writer;
        }
    }
    (void)m_routerBridge.init(::etl::span<::io::IReader*>(readers, 8U),
                              ::etl::span<::io::IWriter*>(writers, 8U));
    transitionDone();
}

void BridgeSupervisor::run()
{
    ::async::scheduleAtFixedRate(m_context, *this, m_timeout, BRIDGE_SUPERVISOR_PERIOD_MS,
                                 ::async::TimeUnit::MILLISECONDS);
    transitionDone();
}

void BridgeSupervisor::shutdown()
{
    m_timeout.cancel();
    m_routerBridge.clear();
    transitionDone();
}

void BridgeSupervisor::execute()
{
    m_timeMs += BRIDGE_SUPERVISOR_PERIOD_MS;
    for (uint32_t i = 0U; i < BRIDGE_MAX_ROUTES_PER_TICK; ++i)
    {
        if (!m_routerBridge.run())
        {
            break;
        }
        m_routedCount++;
    }
    for (auto const& channel : m_config.channels)
    {
        if (channel.pumpTx)
        {
            (void)channel.pumpTx();
        }
    }
}

} // namespace gwbridge
