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
 * \ingroup canstack
 */
#include "canstack/CanSubsystem.h"
#include "canstack/CanStackLogger.h"

namespace canstack
{
namespace logger = ::util::logger;

CanSubsystem& CanSubsystem::getInstance()
{
    static CanSubsystem instance;
    return instance;
}

void CanSubsystem::configure(CanStackConfig const& config, ::async::ContextType const context)
{
    m_config    = config;
    m_context   = context;
    m_configured = true;
}

::async::ContextType CanSubsystem::getTransitionContext(Transition::Type const /* transition */)
{
    return m_context;
}

CanInterface& CanSubsystem::getCanInterface() { return m_canInterface; }

void CanSubsystem::init()
{
    if (!m_configured)
    {
        logger::Logger::error(logger::CANSTACK, "Subsystem not configured");
        transitionDone();
        return;
    }

    (void)m_canInterface.init(m_config);

    transitionDone();
}

void CanSubsystem::run()
{
    if (!m_canInterface.isInitialized())
    {
        transitionDone();
        return;
    }

    ::async::scheduleAtFixedRate(
        m_context,
        *this,
        m_timeout,
        CANSTACK_RUN_PERIOD_MS,
        ::async::TimeUnit::MILLISECONDS);

    logger::Logger::info(logger::CANSTACK, "CAN subsystem running");

    transitionDone();
}

void CanSubsystem::shutdown()
{
    m_timeout.cancel();

    m_canInterface.shutdown();

    logger::Logger::info(logger::CANSTACK, "CAN subsystem stopped");

    transitionDone();
}

void CanSubsystem::execute()
{
    m_canInterface.mainFunctionTx();
    m_canInterface.mainFunctionRx();
}

} // namespace canstack
