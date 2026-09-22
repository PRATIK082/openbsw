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
#include "commgateway/SignalGateway.h"

#include <cmath>

namespace commgateway
{

void SignalGateway::init(::canstack::SignalDatabase* signalDb)
{
    m_signalDb    = signalDb;
    m_initialized = true;
}

void SignalGateway::shutdown()
{
    m_initialized = false;
    m_signalDb    = nullptr;
    clearRules();
}

void SignalGateway::registerRoutingRule(SignalRoutingRule const& rule)
{
    SignalRoutingRule stored = rule;
    // Arm the rate limiter so the first update passes immediately.
    stored.lastTxTimeMs = 0U - stored.minIntervalMs;
    m_routingTable[stored.signalName] = stored;
}

bool SignalGateway::removeRoutingRule(std::string const& signalName)
{
    return m_routingTable.erase(signalName) > 0U;
}

void SignalGateway::clearRules()
{
    m_routingTable.clear();
    while (!m_pendingSignals.empty())
    {
        m_pendingSignals.pop();
    }
}

void SignalGateway::setChannelSender(GatewayChannelSender sender) { m_sender = sender; }

void SignalGateway::onSignalUpdate(std::string const& signalName, double value, uint32_t nowMs)
{
    if (!m_initialized)
    {
        return;
    }
    auto it = m_routingTable.find(signalName);
    if (it == m_routingTable.end())
    {
        return;
    }
    SignalRoutingRule& rule = it->second;
    double routed           = (rule.transform != nullptr) ? rule.transform(value) : value;
    if (std::isnan(routed))
    {
        m_dropCount++;
        return;
    }
    if ((rule.minIntervalMs > 0U) && ((nowMs - rule.lastTxTimeMs) < rule.minIntervalMs))
    {
        PendingSignal pending{};
        pending.signalName = signalName;
        pending.value      = routed;
        pending.dueTimeMs  = rule.lastTxTimeMs + rule.minIntervalMs;
        m_pendingSignals.push(pending);
        return;
    }
    rule.lastTxTimeMs = nowMs;
    dispatch(signalName, routed);
}

void SignalGateway::routeSignal(SignalRoutingRule const& rule, double value)
{
    double routed = (rule.transform != nullptr) ? rule.transform(value) : value;
    if (std::isnan(routed))
    {
        m_dropCount++;
        return;
    }
    dispatch(rule.signalName, routed);
}

void SignalGateway::mainFunction(uint32_t nowMs)
{
    while (!m_pendingSignals.empty())
    {
        PendingSignal const& pending = m_pendingSignals.front();
        auto it                      = m_routingTable.find(pending.signalName);
        if (it == m_routingTable.end())
        {
            m_pendingSignals.pop();
            m_dropCount++;
            continue;
        }
        if ((nowMs - it->second.lastTxTimeMs) < it->second.minIntervalMs)
        {
            break;
        }
        it->second.lastTxTimeMs = nowMs;
        dispatch(pending.signalName, pending.value);
        m_pendingSignals.pop();
    }
}

size_t SignalGateway::getRuleCount() const { return m_routingTable.size(); }

size_t SignalGateway::getPendingCount() const { return m_pendingSignals.size(); }

uint32_t SignalGateway::getDropCount() const { return m_dropCount; }

uint32_t SignalGateway::getForwardCount() const { return m_forwardCount; }

void SignalGateway::dispatch(std::string const& signalName, double value)
{
    auto it = m_routingTable.find(signalName);
    if ((it == m_routingTable.end()) || !m_sender)
    {
        m_dropCount++;
        return;
    }
    for (auto const& destination : it->second.destinations)
    {
        if (m_sender(destination.first, destination.second, signalName, value))
        {
            m_forwardCount++;
        }
        else
        {
            m_dropCount++;
        }
    }
}

} // namespace commgateway
