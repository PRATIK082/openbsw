/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <commchannel/CommChannelManager.h>

#include <ecumode/CommStateManagerPhase5.h>

#include <gtest/gtest.h>

namespace
{

commchannel::CommChannelManager mgr;

void reset()
{
    mgr.shutdown();
    mgr.clear();
    ecumode::CommStateManagerPhase5::getInstance().clear();
    ASSERT_TRUE(mgr.init());
}

} // namespace

TEST(CommChannelManagerTest, InitRejectsInvalidConfig)
{
    commchannel::CommChannelManager m;
    commchannel::CommChannelManagerConfig c{};
    c.maxChannels = 0U;
    EXPECT_FALSE(m.init(c));
    c.maxChannels = 2U;
    c.maxUsersPerChannel = 0U;
    EXPECT_FALSE(m.init(c));
    c.maxUsersPerChannel = 4U;
    EXPECT_TRUE(m.init(c));
}

TEST(CommChannelManagerTest, ChannelRegistration)
{
    reset();
    EXPECT_TRUE(mgr.registerChannel(1U));
    EXPECT_TRUE(mgr.hasChannel(1U));
    EXPECT_EQ(mgr.getChannelCount(), 1U);
    EXPECT_TRUE(mgr.registerChannel(1U)); // idempotent
    EXPECT_EQ(mgr.getChannelCount(), 1U);
    EXPECT_FALSE(mgr.hasChannel(2U));
}

TEST(CommChannelManagerTest, UserRegistration)
{
    reset();
    mgr.registerChannel(1U);
    int8_t u0 = mgr.registerUser(1U);
    int8_t u1 = mgr.registerUser(1U);
    EXPECT_GE(u0, 0);
    EXPECT_GE(u1, 0);
    EXPECT_NE(u0, u1);
    EXPECT_EQ(mgr.getUserCount(1U), 2U);
    EXPECT_TRUE(mgr.isUserRegistered(1U, static_cast<uint8_t>(u0)));
}

TEST(CommChannelManagerTest, RequestAggregationHighestMode)
{
    reset();
    mgr.registerChannel(1U);
    int8_t u0 = mgr.registerUser(1U);
    int8_t u1 = mgr.registerUser(1U);
    mgr.requestCommunication(static_cast<uint8_t>(u0), 1U, commchannel::CommMode::Silent);
    mgr.mainFunction(10U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::Silent);

    mgr.requestCommunication(static_cast<uint8_t>(u1), 1U, commchannel::CommMode::Full);
    mgr.mainFunction(20U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::Full);

    mgr.releaseCommunication(static_cast<uint8_t>(u1), 1U);
    mgr.mainFunction(30U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::Silent);
}

TEST(CommChannelManagerTest, ReleaseAllGoesNo)
{
    reset();
    mgr.registerChannel(1U);
    int8_t u0 = mgr.registerUser(1U);
    mgr.requestCommunication(static_cast<uint8_t>(u0), 1U, commchannel::CommMode::Full);
    mgr.mainFunction(10U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::Full);
    mgr.releaseCommunication(static_cast<uint8_t>(u0), 1U);
    mgr.mainFunction(20U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::No);
}

TEST(CommChannelManagerTest, NoCommEcuModeDisables)
{
    reset();
    mgr.registerChannel(1U);
    int8_t u0 = mgr.registerUser(1U);
    mgr.requestCommunication(static_cast<uint8_t>(u0), 1U, commchannel::CommMode::Full);
    // disable communication in NORMAL mode
    mgr.setModeCommunication(ecumode::EcuMode::NORMAL, false);
    mgr.setEcuMode(ecumode::EcuMode::NORMAL);
    mgr.mainFunction(10U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::No);
}

TEST(CommChannelManagerTest, ForcedMode)
{
    reset();
    mgr.registerChannel(1U);
    mgr.setChannelMode(1U, commchannel::CommMode::Silent);
    mgr.mainFunction(10U);
    EXPECT_EQ(mgr.getChannelMode(1U), commchannel::CommMode::Silent);
}

TEST(CommChannelManagerTest, ChannelFirstConvenience)
{
    reset();
    mgr.registerChannel(2U);
    mgr.registerUser(2U, 5U);
    mgr.requestCommunicationOnChannel(2U, 5U, commchannel::CommMode::Full);
    mgr.mainFunction(10U);
    EXPECT_EQ(mgr.getChannelMode(2U), commchannel::CommMode::Full);
    mgr.releaseCommunicationOnChannel(2U, 5U);
    mgr.mainFunction(20U);
    EXPECT_EQ(mgr.getChannelMode(2U), commchannel::CommMode::No);
}

TEST(CommChannelManagerTest, ParseCommMode)
{
    commchannel::CommMode m;
    EXPECT_TRUE(commchannel::parseCommMode("FULL", m));
    EXPECT_EQ(m, commchannel::CommMode::Full);
    EXPECT_TRUE(commchannel::parseCommMode("silent", m));
    EXPECT_EQ(m, commchannel::CommMode::Silent);
    EXPECT_FALSE(commchannel::parseCommMode("nope", m));
}

TEST(CommChannelManagerTest, Constants)
{
    EXPECT_EQ(commchannel::COMM_FULL_COMMUNICATION, commchannel::CommMode::Full);
    EXPECT_EQ(commchannel::COMM_NO_COMMUNICATION, commchannel::CommMode::No);
    EXPECT_EQ(commchannel::COMM_SILENT_COMMUNICATION, commchannel::CommMode::Silent);
}
