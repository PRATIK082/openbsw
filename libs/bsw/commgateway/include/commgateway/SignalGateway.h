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

#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace canstack
{
class SignalDatabase;
}

namespace commgateway
{

/// Channel type ids used in routing rules: 0 = CAN, 1 = LIN, 2 = ETH.
enum ChannelType : uint8_t
{
    CHANNEL_TYPE_CAN = 0U,
    CHANNEL_TYPE_LIN = 1U,
    CHANNEL_TYPE_ETH = 2U
};

/// Optional signal transformation; returning NaN drops the signal.
using SignalTransform = std::function<double(double)>;

/// One signal routing rule: signal name to (channelType, channelId) targets.
struct SignalRoutingRule
{
    std::string signalName;
    std::vector<std::pair<uint8_t, uint32_t>> destinations;
    SignalTransform transform;
    /// Minimum time between two forwarded updates; 0 = no rate limiting.
    uint32_t minIntervalMs = 0U;
    uint32_t lastTxTimeMs  = 0U;
};

/// Channel dispatch: (channelType, channelId, signalName, value) -> accepted.
using GatewayChannelSender = std::function<bool(uint8_t, uint32_t, std::string const&, double)>;

/**
 * Protocol-agnostic signal router.
 *
 * Any protocol layer reports signal updates via onSignalUpdate(); the
 * gateway applies the rule's transform and rate limiting and dispatches to
 * the registered channel sender. Rate-limited signals wait in a pending
 * queue drained by mainFunction().
 */
class SignalGateway
{
public:
    SignalGateway() = default;

    void init(::canstack::SignalDatabase* signalDb);
    void shutdown();

    void registerRoutingRule(SignalRoutingRule const& rule);
    bool removeRoutingRule(std::string const& signalName);
    void clearRules();
    void setChannelSender(GatewayChannelSender sender);

    /// Called by any protocol layer when a signal value changes.
    void onSignalUpdate(std::string const& signalName, double value, uint32_t nowMs);
    /// Immediate dispatch of one rule, bypassing rate limiting.
    void routeSignal(SignalRoutingRule const& rule, double value);

    /// Forwards due pending signals; call every millisecond.
    void mainFunction(uint32_t nowMs);

    size_t getRuleCount() const;
    size_t getPendingCount() const;
    uint32_t getDropCount() const;
    uint32_t getForwardCount() const;

private:
    struct PendingSignal
    {
        std::string signalName;
        double value;
        uint32_t dueTimeMs;
    };

    void dispatch(std::string const& signalName, double value);

    ::canstack::SignalDatabase* m_signalDb = nullptr;
    std::map<std::string, SignalRoutingRule> m_routingTable;
    std::queue<PendingSignal> m_pendingSignals;
    GatewayChannelSender m_sender;
    bool m_initialized  = false;
    uint32_t m_dropCount    = 0U;
    uint32_t m_forwardCount = 0U;
};

} // namespace commgateway
