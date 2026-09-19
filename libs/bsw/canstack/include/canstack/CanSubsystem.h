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
#pragma once

#include "canstack/CanInterface.h"

#include <async/Async.h>
#include <lifecycle/SimpleLifecycleComponent.h>

namespace canstack
{

/// Run period of the stack's main functions in milliseconds.
static uint32_t const CANSTACK_RUN_PERIOD_MS = 1U;

/**
 * Lifecycle integration of the CAN stack.
 *
 * Binds CanInterface to the OpenBSW lifecycle:
 *
 * - init(): builds the stack from the configuration.
 * - run(): schedules the periodic main functions (tx scheduler, rx
 *   processing) with a 1 ms period in the component's async context; the
 *   OSAL maps the context to a FreeRTOS/ThreadX task.
 * - shutdown(): cancels the periodic execution and stops the stack.
 *
 * Register the subsystem with the LifecycleManager like any other
 * LifecycleComponent:
 * \code{.cpp}
 * auto& subsystem = CanSubsystem::getInstance();
 * lifecycleManager.registerComponent(subsystem);
 * \endcode
 */
class CanSubsystem
: public ::lifecycle::SimpleLifecycleComponent
, private ::async::RunnableType
{
public:
    /// \return the process wide subsystem instance.
    static CanSubsystem& getInstance();

    /**
     * \param config code-first stack configuration (constexpr tables plus
     * channel drivers)
     * \param context async context the periodic main functions run in
     */
    void configure(CanStackConfig const& config, ::async::ContextType context);

    ::async::ContextType getTransitionContext(Transition::Type transition) override;

    /// \return the stack API (valid after configure()).
    CanInterface& getCanInterface();

protected:
    ~CanSubsystem() override = default;

    // ::lifecycle::SimpleLifecycleComponent
    void init() override;
    void run() override;
    void shutdown() override;

private:
    // ::async::RunnableType
    void execute() override;

    CanInterface m_canInterface;
    CanStackConfig m_config{};
    ::async::ContextType m_context = ::async::CONTEXT_INVALID;
    ::async::TimeoutType m_timeout;
    bool m_configured = false;
};

} // namespace canstack
