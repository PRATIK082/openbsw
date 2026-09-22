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

#include <cstdint>
#include <functional>
#include <vector>

namespace framegateway
{

/// Policy verdict for one frame.
enum class PolicyAction : uint8_t
{
    ALLOW,
    DENY,
    TRANSFORM,
    RATE_LIMIT,
    LOG_ONLY,
    REDIRECT
};

/// One routing policy rule for a (channelType, channelId, frameId) triple.
struct GatewayPolicy
{
    uint32_t frameId    = 0U;
    uint8_t channelType = 0U;
    uint8_t channelId   = 0U;
    PolicyAction action  = PolicyAction::ALLOW;
    /**
     * Match predicate: true = the rule applies to this frame, false = the
     * rule is skipped (frame allowed). Null = always applies.
     * Example DENY rule: [](data, len) { return data[0] != 0xF1; }
     */
    std::function<bool(uint8_t const*, uint16_t)> filter;
    /// In-place frame modifier for TRANSFORM. Null = no modification.
    std::function<void(uint8_t*, uint16_t)> transform;
    /// Redirect target for REDIRECT (channelType, channelId, frameId).
    uint32_t redirectFrameId   = 0U;
    uint8_t redirectChannelType = 0U;
    uint8_t redirectChannelId   = 0U;
    /// Rate budget for RATE_LIMIT; 0 = deny everything after the first per window.
    uint32_t maxRatePerSec = 0U;
    uint32_t currentRate   = 0U;
    uint32_t lastRateResetTimeMs = 0U;
};

/// Log hook for LOG_ONLY / denied frames: (frameId, length, data).
using PolicyLogHook = std::function<void(uint32_t, uint16_t, uint8_t const*)>;

/**
 * Routing policy engine: filter, transform, rate-limit, security rules.
 *
 * The first matching rule wins; frames without a rule are allowed.
 * evaluatePolicy() may modify data/length in place (TRANSFORM) or rewrite
 * the frame key (REDIRECT). mainFunction() resets the 1 s rate windows.
 */
class PolicyEngine
{
public:
    PolicyEngine() = default;

    void addPolicy(GatewayPolicy const& policy);
    bool removePolicy(uint8_t channelType, uint8_t channelId, uint32_t frameId);
    void clear();
    void setLogHook(PolicyLogHook hook);

    /**
     * \return verdict; for REDIRECT channelType/channelId/frameId carry the
     * new key, for DENY the frame must be dropped.
     */
    PolicyAction evaluatePolicy(uint8_t& channelType, uint8_t& channelId, uint32_t& frameId,
                                uint8_t* data, uint16_t& length, uint32_t nowMs);

    /// Resets rate-limit windows; call every millisecond.
    void mainFunction(uint32_t nowMs);

    size_t getPolicyCount() const;
    uint32_t getDenyCount() const;

private:
    GatewayPolicy* findPolicy(uint8_t channelType, uint8_t channelId, uint32_t frameId);

    std::vector<GatewayPolicy> m_policies;
    PolicyLogHook m_logHook;
    uint32_t m_denyCount = 0U;
};

} // namespace framegateway
