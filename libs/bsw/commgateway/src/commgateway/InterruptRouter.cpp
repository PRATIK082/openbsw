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
#include "commgateway/InterruptRouter.h"

namespace commgateway
{

InterruptRouter& InterruptRouter::getInstance()
{
    static InterruptRouter instance;
    return instance;
}

void InterruptRouter::registerHandler(InterruptSource source, std::function<void()> handler)
{
    m_handlers[source] = handler;
}

bool InterruptRouter::removeHandler(InterruptSource source) { return m_handlers.erase(source) > 0U; }

void InterruptRouter::clear() { m_handlers.clear(); }

void InterruptRouter::onInterrupt(InterruptSource source)
{
    auto it = m_handlers.find(source);
    if ((it != m_handlers.end()) && (it->second != nullptr))
    {
        it->second();
    }
}

void InterruptRouter::onCan0RxIsr() { getInstance().onInterrupt(InterruptSource::CAN0_RX); }
void InterruptRouter::onCan0TxIsr() { getInstance().onInterrupt(InterruptSource::CAN0_TX); }
void InterruptRouter::onCan0ErrorIsr() { getInstance().onInterrupt(InterruptSource::CAN0_ERROR); }
void InterruptRouter::onLin0RxIsr() { getInstance().onInterrupt(InterruptSource::LIN0_RX); }
void InterruptRouter::onLin0TxIsr() { getInstance().onInterrupt(InterruptSource::LIN0_TX); }
void InterruptRouter::onLin0ErrorIsr() { getInstance().onInterrupt(InterruptSource::LIN0_ERROR); }
void InterruptRouter::onEth0RxIsr() { getInstance().onInterrupt(InterruptSource::ETH0_RX); }
void InterruptRouter::onEth0TxIsr() { getInstance().onInterrupt(InterruptSource::ETH0_TX); }
void InterruptRouter::onEth0ErrorIsr() { getInstance().onInterrupt(InterruptSource::ETH0_ERROR); }

size_t InterruptRouter::getHandlerCount() const { return m_handlers.size(); }

} // namespace commgateway
