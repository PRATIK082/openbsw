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

#include <cstddef>
#include <functional>
#include <map>

namespace commgateway
{

/// Interrupt sources fanned out by the router.
enum class InterruptSource : uint8_t
{
    CAN0_RX,
    CAN0_TX,
    CAN0_ERROR,
    LIN0_RX,
    LIN0_TX,
    LIN0_ERROR,
    ETH0_RX,
    ETH0_TX,
    ETH0_ERROR
};

/**
 * Centralized interrupt event distribution.
 *
 * BSP interrupt vectors call the static onXxxIsr() wrappers (or onInterrupt()
 * directly); registered handlers run in caller context, so handlers must stay
 * ISR safe (set flags, forward to driver queues).
 */
class InterruptRouter
{
public:
    static InterruptRouter& getInstance();

    void registerHandler(InterruptSource source, std::function<void()> handler);
    bool removeHandler(InterruptSource source);
    void clear();
    /// Dispatches to the registered handler; no-op when none is registered.
    void onInterrupt(InterruptSource source);

    static void onCan0RxIsr();
    static void onCan0TxIsr();
    static void onCan0ErrorIsr();
    static void onLin0RxIsr();
    static void onLin0TxIsr();
    static void onLin0ErrorIsr();
    static void onEth0RxIsr();
    static void onEth0TxIsr();
    static void onEth0ErrorIsr();

    size_t getHandlerCount() const;

private:
    InterruptRouter() = default;

    std::map<InterruptSource, std::function<void()>> m_handlers;
};

} // namespace commgateway
