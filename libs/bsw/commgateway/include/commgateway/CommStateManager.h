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
#include <utility>

namespace commgateway
{

/// Channel communication state.
enum class CommState : uint8_t
{
    UNINIT,
    ACTIVE,
    PASSIVE,
    BUS_OFF,
    COMM_FAILURE
};

/// Per-channel state with error budget and auto-recovery option.
struct ChannelState
{
    uint8_t channelType = 0U;
    uint8_t channelId   = 0U;
    CommState state     = CommState::UNINIT;
    uint32_t errorCounter       = 0U;
    uint32_t lastStateChangeTimeMs = 0U;
    std::function<void(CommState)> stateChangeCallback;
    bool autoRecovery       = false;
    uint32_t recoveryTimeMs = 1000U;
};

/// Error counter threshold moving a CAN channel ACTIVE -> BUS_OFF.
static uint32_t const COMM_CAN_BUS_OFF_THRESHOLD = 255U;
/// Error counter threshold moving a LIN channel ACTIVE -> PASSIVE.
static uint32_t const COMM_LIN_PASSIVE_THRESHOLD = 10U;

/**
 * Channel state machine for all bus systems.
 *
 * reportError() counts bus errors and derives standard transitions
 * (CAN -> BUS_OFF past 255 errors, LIN -> PASSIVE past threshold,
 * ETH -> COMM_FAILURE immediately). With autoRecovery enabled,
 * mainFunction() returns BUS_OFF channels to ACTIVE after recoveryTimeMs.
 */
class CommStateManager
{
public:
    CommStateManager() = default;

    void init();
    void shutdown();
    void clear();

    void registerChannel(uint8_t channelType, uint8_t channelId);
    void updateChannelState(uint8_t channelType, uint8_t channelId, CommState newState,
                            uint32_t nowMs = 0U);
    CommState getChannelState(uint8_t channelType, uint8_t channelId) const;
    void setGlobalState(CommState state, uint32_t nowMs = 0U);
    void reportError(uint8_t channelType, uint8_t channelId, uint32_t nowMs = 0U);
    void setStateChangeCallback(uint8_t channelType, uint8_t channelId,
                                std::function<void(CommState)> cb);
    void setRecoveryConfig(uint8_t channelType, uint8_t channelId, bool autoRecovery,
                           uint32_t recoveryTimeMs);

    /// Auto-recovery handling; call every millisecond.
    void mainFunction(uint32_t nowMs);

    size_t getChannelCount() const;
    uint32_t getErrorCount(uint8_t channelType, uint8_t channelId) const;

private:
    using Key = std::pair<uint8_t, uint8_t>;

    ChannelState* findChannel(uint8_t channelType, uint8_t channelId);
    ChannelState const* findChannel(uint8_t channelType, uint8_t channelId) const;

    std::map<Key, ChannelState> m_channelStates;
    CommState m_globalState = CommState::UNINIT;
    bool m_initialized      = false;
};

} // namespace commgateway
