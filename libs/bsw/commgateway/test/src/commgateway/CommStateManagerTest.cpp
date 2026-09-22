/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/CommStateManager.h"
#include "commgateway/SignalGateway.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

/**
 * \desc: CAN goes BUS_OFF past 255 errors; LIN goes PASSIVE past threshold;
 * ETH fails immediately.
 */
TEST(CommStateManagerTest, error_transitions)
{
    ::commgateway::CommStateManager states;
    states.init();
    states.registerChannel(::commgateway::CHANNEL_TYPE_CAN, 0U);
    states.registerChannel(::commgateway::CHANNEL_TYPE_LIN, 0U);
    states.registerChannel(::commgateway::CHANNEL_TYPE_ETH, 0U);
    EXPECT_EQ(3U, states.getChannelCount());

    for (uint32_t i = 0U; i < 255U; ++i)
    {
        states.reportError(::commgateway::CHANNEL_TYPE_CAN, 0U);
    }
    EXPECT_EQ(::commgateway::CommState::ACTIVE,
              states.getChannelState(::commgateway::CHANNEL_TYPE_CAN, 0U));
    states.reportError(::commgateway::CHANNEL_TYPE_CAN, 0U);
    EXPECT_EQ(::commgateway::CommState::BUS_OFF,
              states.getChannelState(::commgateway::CHANNEL_TYPE_CAN, 0U));

    for (uint32_t i = 0U; i <= ::commgateway::COMM_LIN_PASSIVE_THRESHOLD; ++i)
    {
        states.reportError(::commgateway::CHANNEL_TYPE_LIN, 0U);
    }
    EXPECT_EQ(::commgateway::CommState::PASSIVE,
              states.getChannelState(::commgateway::CHANNEL_TYPE_LIN, 0U));

    states.reportError(::commgateway::CHANNEL_TYPE_ETH, 0U);
    EXPECT_EQ(::commgateway::CommState::COMM_FAILURE,
              states.getChannelState(::commgateway::CHANNEL_TYPE_ETH, 0U));
}

/**
 * \desc: Auto-recovery returns BUS_OFF channels to ACTIVE after the delay.
 */
TEST(CommStateManagerTest, auto_recovery)
{
    ::commgateway::CommStateManager states;
    states.init();
    states.registerChannel(::commgateway::CHANNEL_TYPE_CAN, 1U);
    states.setRecoveryConfig(::commgateway::CHANNEL_TYPE_CAN, 1U, true, 1000U);

    ::commgateway::CommState lastSeen = ::commgateway::CommState::UNINIT;
    states.setStateChangeCallback(::commgateway::CHANNEL_TYPE_CAN, 1U,
                                  [&lastSeen](::commgateway::CommState s) { lastSeen = s; });

    states.updateChannelState(::commgateway::CHANNEL_TYPE_CAN, 1U,
                              ::commgateway::CommState::BUS_OFF, 0U);
    EXPECT_EQ(::commgateway::CommState::BUS_OFF, lastSeen);

    states.mainFunction(999U);
    EXPECT_EQ(::commgateway::CommState::BUS_OFF,
              states.getChannelState(::commgateway::CHANNEL_TYPE_CAN, 1U));
    states.mainFunction(1000U);
    EXPECT_EQ(::commgateway::CommState::ACTIVE,
              states.getChannelState(::commgateway::CHANNEL_TYPE_CAN, 1U));
    EXPECT_EQ(::commgateway::CommState::ACTIVE, lastSeen);
}

/**
 * \desc: Global state applies to every registered channel.
 */
TEST(CommStateManagerTest, global_state)
{
    ::commgateway::CommStateManager states;
    states.init();
    states.registerChannel(0U, 0U);
    states.registerChannel(1U, 0U);
    states.setGlobalState(::commgateway::CommState::PASSIVE, 0U);
    EXPECT_EQ(::commgateway::CommState::PASSIVE, states.getChannelState(0U, 0U));
    EXPECT_EQ(::commgateway::CommState::PASSIVE, states.getChannelState(1U, 0U));
    EXPECT_EQ(::commgateway::CommState::UNINIT, states.getChannelState(9U, 9U));
}

} // namespace
