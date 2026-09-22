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
#include "commgateway/CommStateManager.h"
#include "commgateway/SignalGateway.h"

namespace commgateway
{

void CommStateManager::init()
{
    m_globalState = CommState::UNINIT;
    m_initialized = true;
}

void CommStateManager::shutdown()
{
    m_initialized = false;
    clear();
}

void CommStateManager::clear()
{
    m_channelStates.clear();
    m_globalState = CommState::UNINIT;
}

void CommStateManager::registerChannel(uint8_t channelType, uint8_t channelId)
{
    Key key(channelType, channelId);
    if (m_channelStates.find(key) != m_channelStates.end())
    {
        return;
    }
    ChannelState state{};
    state.channelType = channelType;
    state.channelId   = channelId;
    state.state       = CommState::ACTIVE;
    m_channelStates[key] = state;
}

void CommStateManager::updateChannelState(uint8_t channelType, uint8_t channelId,
                                          CommState newState, uint32_t nowMs)
{
    ChannelState* channel = findChannel(channelType, channelId);
    if (channel == nullptr)
    {
        return;
    }
    if (channel->state != newState)
    {
        channel->state                = newState;
        channel->lastStateChangeTimeMs = nowMs;
        if (channel->stateChangeCallback)
        {
            channel->stateChangeCallback(newState);
        }
    }
}

CommState CommStateManager::getChannelState(uint8_t channelType, uint8_t channelId) const
{
    ChannelState const* channel = findChannel(channelType, channelId);
    return (channel != nullptr) ? channel->state : CommState::UNINIT;
}

void CommStateManager::setGlobalState(CommState state, uint32_t nowMs)
{
    m_globalState = state;
    for (auto& entry : m_channelStates)
    {
        updateChannelState(entry.second.channelType, entry.second.channelId, state, nowMs);
    }
}

void CommStateManager::reportError(uint8_t channelType, uint8_t channelId, uint32_t nowMs)
{
    ChannelState* channel = findChannel(channelType, channelId);
    if (channel == nullptr)
    {
        return;
    }
    channel->errorCounter++;
    if (channelType == CHANNEL_TYPE_CAN)
    {
        if (channel->errorCounter > COMM_CAN_BUS_OFF_THRESHOLD)
        {
            updateChannelState(channelType, channelId, CommState::BUS_OFF, nowMs);
        }
    }
    else if (channelType == CHANNEL_TYPE_LIN)
    {
        if (channel->errorCounter > COMM_LIN_PASSIVE_THRESHOLD)
        {
            updateChannelState(channelType, channelId, CommState::PASSIVE, nowMs);
        }
    }
    else
    {
        updateChannelState(channelType, channelId, CommState::COMM_FAILURE, nowMs);
    }
}

void CommStateManager::setStateChangeCallback(uint8_t channelType, uint8_t channelId,
                                              std::function<void(CommState)> cb)
{
    ChannelState* channel = findChannel(channelType, channelId);
    if (channel != nullptr)
    {
        channel->stateChangeCallback = cb;
    }
}

void CommStateManager::setRecoveryConfig(uint8_t channelType, uint8_t channelId, bool autoRecovery,
                                         uint32_t recoveryTimeMs)
{
    ChannelState* channel = findChannel(channelType, channelId);
    if (channel != nullptr)
    {
        channel->autoRecovery   = autoRecovery;
        channel->recoveryTimeMs = recoveryTimeMs;
    }
}

void CommStateManager::mainFunction(uint32_t nowMs)
{
    if (!m_initialized)
    {
        return;
    }
    for (auto& entry : m_channelStates)
    {
        ChannelState& channel = entry.second;
        if (channel.autoRecovery && (channel.state == CommState::BUS_OFF)
            && ((nowMs - channel.lastStateChangeTimeMs) >= channel.recoveryTimeMs))
        {
            channel.errorCounter = 0U;
            updateChannelState(channel.channelType, channel.channelId, CommState::ACTIVE, nowMs);
        }
    }
}

size_t CommStateManager::getChannelCount() const { return m_channelStates.size(); }

uint32_t CommStateManager::getErrorCount(uint8_t channelType, uint8_t channelId) const
{
    ChannelState const* channel = findChannel(channelType, channelId);
    return (channel != nullptr) ? channel->errorCounter : 0U;
}

ChannelState* CommStateManager::findChannel(uint8_t channelType, uint8_t channelId)
{
    auto it = m_channelStates.find(Key(channelType, channelId));
    return (it != m_channelStates.end()) ? &it->second : nullptr;
}

ChannelState const* CommStateManager::findChannel(uint8_t channelType,
                                                            uint8_t channelId) const
{
    auto it = m_channelStates.find(Key(channelType, channelId));
    return (it != m_channelStates.end()) ? &it->second : nullptr;
}

} // namespace commgateway
