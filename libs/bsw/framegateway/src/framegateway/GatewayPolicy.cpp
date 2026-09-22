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
#include "framegateway/GatewayPolicy.h"

namespace framegateway
{

void PolicyEngine::addPolicy(GatewayPolicy const& policy)
{
    for (auto& existing : m_policies)
    {
        if ((existing.channelType == policy.channelType)
            && (existing.channelId == policy.channelId) && (existing.frameId == policy.frameId))
        {
            existing = policy;
            return;
        }
    }
    m_policies.push_back(policy);
}

bool PolicyEngine::removePolicy(uint8_t channelType, uint8_t channelId, uint32_t frameId)
{
    for (auto it = m_policies.begin(); it != m_policies.end(); ++it)
    {
        if ((it->channelType == channelType) && (it->channelId == channelId)
            && (it->frameId == frameId))
        {
            m_policies.erase(it);
            return true;
        }
    }
    return false;
}

void PolicyEngine::clear()
{
    m_policies.clear();
    m_denyCount = 0U;
}

void PolicyEngine::setLogHook(PolicyLogHook hook) { m_logHook = hook; }

PolicyAction PolicyEngine::evaluatePolicy(uint8_t& channelType, uint8_t& channelId,
                                          uint32_t& frameId, uint8_t* data, uint16_t& length,
                                          uint32_t nowMs)
{
    GatewayPolicy* policy = findPolicy(channelType, channelId, frameId);
    if (policy == nullptr)
    {
        return PolicyAction::ALLOW;
    }
    if ((policy->filter != nullptr) && ((data == nullptr) || !policy->filter(data, length)))
    {
        return PolicyAction::ALLOW; // rule does not apply to this frame
    }

    if ((policy->action == PolicyAction::RATE_LIMIT) && (policy->maxRatePerSec > 0U))
    {
        if (((nowMs - policy->lastRateResetTimeMs) >= 1000U)
            || (nowMs < policy->lastRateResetTimeMs))
        {
            policy->lastRateResetTimeMs = nowMs;
            policy->currentRate         = 0U;
        }
        policy->currentRate++;
        if (policy->currentRate > policy->maxRatePerSec)
        {
            m_denyCount++;
            return PolicyAction::DENY;
        }
        return PolicyAction::ALLOW;
    }

    switch (policy->action)
    {
        case PolicyAction::DENY:
            m_denyCount++;
            if (m_logHook)
            {
                m_logHook(frameId, length, data);
            }
            return PolicyAction::DENY;
        case PolicyAction::TRANSFORM:
            if ((policy->transform != nullptr) && (data != nullptr))
            {
                policy->transform(data, length);
            }
            return PolicyAction::ALLOW;
        case PolicyAction::LOG_ONLY:
            if (m_logHook)
            {
                m_logHook(frameId, length, data);
            }
            return PolicyAction::ALLOW;
        case PolicyAction::REDIRECT:
            channelType = policy->redirectChannelType;
            channelId   = policy->redirectChannelId;
            frameId     = policy->redirectFrameId;
            return PolicyAction::ALLOW;
        case PolicyAction::RATE_LIMIT:
            m_denyCount++;
            return PolicyAction::DENY;
        case PolicyAction::ALLOW:
        default:
            return PolicyAction::ALLOW;
    }
}

void PolicyEngine::mainFunction(uint32_t nowMs)
{
    for (auto& policy : m_policies)
    {
        if (((nowMs - policy.lastRateResetTimeMs) >= 1000U)
            || (nowMs < policy.lastRateResetTimeMs))
        {
            policy.lastRateResetTimeMs = nowMs;
            policy.currentRate         = 0U;
        }
    }
}

size_t PolicyEngine::getPolicyCount() const { return m_policies.size(); }

uint32_t PolicyEngine::getDenyCount() const { return m_denyCount; }

GatewayPolicy* PolicyEngine::findPolicy(uint8_t channelType, uint8_t channelId, uint32_t frameId)
{
    for (auto& policy : m_policies)
    {
        if ((policy.channelType == channelType) && (policy.channelId == channelId)
            && (policy.frameId == frameId))
        {
            return &policy;
        }
    }
    return nullptr;
}

} // namespace framegateway
