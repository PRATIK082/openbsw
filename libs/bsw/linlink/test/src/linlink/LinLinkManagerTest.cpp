/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <linlink/LinLinkManager.h>
#include <linlink/LinLinkManager.h>

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace
{

linlink::LinLinkManagerConfig makeCfg(uint8_t nodeId = 1U)
{
    linlink::LinLinkManagerConfig c{};
    c.nodeId = nodeId;
    c.nmPduId = 0x500U;
    c.userDataLength = 4U;
    c.tNm = 2000U;
    c.tRepeatMessage = 1000U;
    c.tWaitBusSleep = 1500U;
    c.tTx = 500U;
    c.activeNm = true;
    c.maxNodes = 8U;
    return c;
}

} // namespace

TEST(LinLinkManagerTest, InitRejectsInvalidConfig)
{
    linlink::LinLinkManager nm;
    linlink::LinLinkManagerConfig c = makeCfg();
    c.maxNodes = 0U;
    EXPECT_FALSE(nm.init(c));
    c = makeCfg();
    c.tTx = 0U;
    EXPECT_FALSE(nm.init(c));
    c = makeCfg();
    c.userDataLength = 7U;
    EXPECT_FALSE(nm.init(c));
}

TEST(LinLinkManagerTest, EncodeDecodeRoundTrip)
{
    linlink::LinLinkManagerConfig c = makeCfg(3U);
    linlink::NmControlBits cb{};
    cb.repeatMessageRequest = true;
    cb.activeWakeup = true;
    uint8_t ud[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t buf[8] = {};
    uint8_t dlc = linlink::LinLinkManager::encodePdu(c, cb, 3U, ud, buf, 8U);
    EXPECT_EQ(dlc, 6U); // 2 + 4

    linlink::NmControlBits cb2{};
    uint8_t src = 0U;
    uint8_t ud2[8] = {};
    uint8_t len = 0U;
    ASSERT_TRUE(linlink::LinLinkManager::decodePdu(buf, dlc, cb2, src, ud2, len));
    EXPECT_TRUE(cb2.repeatMessageRequest);
    EXPECT_FALSE(cb2.nmCoordinatorSleepReady);
    EXPECT_TRUE(cb2.activeWakeup);
    EXPECT_EQ(src, 3U);
    EXPECT_EQ(len, 4U);
    EXPECT_EQ(std::memcmp(ud, ud2, 4), 0);
}

TEST(LinLinkManagerTest, DecodeRejectsTooSmall)
{
    uint8_t buf[1] = {0};
    linlink::NmControlBits cb{};
    uint8_t src = 0;
    uint8_t ud[8] = {};
    uint8_t len = 0;
    EXPECT_FALSE(linlink::LinLinkManager::decodePdu(buf, 1, cb, src, ud, len));
    EXPECT_FALSE(linlink::LinLinkManager::decodePdu(nullptr, 2, cb, src, ud, len));
}

TEST(LinLinkManagerTest, InitialStateBusSleep)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    EXPECT_EQ(nm.getState(), linlink::NmState::BusSleep);
    EXPECT_FALSE(nm.isNetworkRequested());
}

TEST(LinLinkManagerTest, NetworkRequestWakesToRepeatMessage)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    bool woken = false;
    nm.setWakeupCallback([&]() { woken = true; });
    nm.networkRequest();
    EXPECT_TRUE(nm.isNetworkRequested());
    nm.mainFunction(0U);
    EXPECT_EQ(nm.getState(), linlink::NmState::RepeatMessage);
    EXPECT_TRUE(woken);
}

TEST(LinLinkManagerTest, NetworkReleaseEventuallySleeps)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(nm.getState(), linlink::NmState::RepeatMessage);

    nm.networkRelease();
    EXPECT_FALSE(nm.isNetworkRequested());
    // stay in repeat for tRepeatMessage
    nm.mainFunction(500U);
    // after tRepeatMessage (1000ms) with no request -> ReadySleep
    nm.mainFunction(1100U);
    EXPECT_EQ(nm.getState(), linlink::NmState::ReadySleep);
}

TEST(LinLinkManagerTest, ReceivedPduWakesFromBusSleep)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    EXPECT_EQ(nm.getState(), linlink::NmState::BusSleep);

    uint8_t buf[6] = {0, 2, 0, 0, 0, 0}; // node 2, empty ud
    nm.onNmPduReceived(2U, buf, 6U, 100U);
    EXPECT_EQ(nm.getState(), linlink::NmState::RepeatMessage);
}

TEST(LinLinkManagerTest, NodeTrackingAliveAndDead)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    uint8_t buf[6] = {0, 2, 0x11, 0x22, 0x33, 0x44};
    nm.onNmPduReceived(2U, buf, 6U, 100U);
    EXPECT_TRUE(nm.isNodeAlive(2U, 100U));
    EXPECT_TRUE(nm.isNodeAlive(2U, 2000U)); // within tNm (2000ms from 100 -> 2100)
    EXPECT_FALSE(nm.isNodeAlive(2U, 2200U)); // expired

    uint8_t ud[4] = {};
    nm.getNodeUserData(2U, ud, 4U);
    EXPECT_EQ(ud[0], 0x11);
}

TEST(LinLinkManagerTest, AliveNodeCount)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg(1U)));
    nm.networkRequest();
    nm.mainFunction(0U); // self active in RepeatMessage
    EXPECT_GE(nm.getAliveNodeCount(), 1U);

    uint8_t buf[6] = {0, 2, 0, 0, 0, 0};
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    EXPECT_GE(nm.getAliveNodeCount(), 2U);
}

TEST(LinLinkManagerTest, ReadySleepToPrepareBusSleepWhenAlone)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    nm.networkRelease();
    nm.mainFunction(1100U); // -> ReadySleep
    EXPECT_EQ(nm.getState(), linlink::NmState::ReadySleep);
    // no other nodes, tNm expired relative to last rx (0) -> PrepareBusSleep
    nm.mainFunction(5000U);
    EXPECT_EQ(nm.getState(), linlink::NmState::PrepareBusSleep);
}

TEST(LinLinkManagerTest, PrepareBusSleepTimesOutToBusSleep)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    nm.networkRelease();
    nm.mainFunction(1100U); // ReadySleep
    nm.mainFunction(5000U); // PrepareBusSleep
    EXPECT_EQ(nm.getState(), linlink::NmState::PrepareBusSleep);
    bool slept = false;
    nm.setSleepCallback([&]() { slept = true; });
    nm.mainFunction(5000U + 1600U);
    EXPECT_EQ(nm.getState(), linlink::NmState::BusSleep);
    EXPECT_TRUE(slept);
}

TEST(LinLinkManagerTest, TxCallbackInvokedInRepeatMessage)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    int txCount = 0;
    nm.setTxCallback([&](uint32_t id, uint8_t dlc, uint8_t const* /*data*/) {
        EXPECT_EQ(id, 0x500U);
        EXPECT_GE(dlc, 2U);
        ++txCount;
        return true;
    });
    nm.networkRequest();
    nm.mainFunction(0U); // state entry; tx on first mainFunction (lastTxTime_ 0)
    EXPECT_GE(txCount, 1);
}

TEST(LinLinkManagerTest, ParseNmState)
{
    linlink::NmState s;
    EXPECT_TRUE(linlink::parseNmState("BusSleep", s));
    EXPECT_EQ(s, linlink::NmState::BusSleep);
    EXPECT_FALSE(linlink::parseNmState("Nope", s));
    EXPECT_FALSE(linlink::parseNmState(nullptr, s));
}

TEST(LinLinkManagerTest, UserDataLocal)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    uint8_t in[4] = {1, 2, 3, 4};
    nm.setUserData(in, 4);
    uint8_t out[4] = {};
    nm.getUserData(out, 4);
    EXPECT_EQ(std::memcmp(in, out, 4), 0);
}

TEST(LinLinkManagerTest, UserManagementAggregation)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.registerUser();
    EXPECT_FALSE(nm.isNetworkRequested());
    nm.userNetworkRequest(0U);
    nm.userNetworkRequest(1U);
    EXPECT_TRUE(nm.isNetworkRequested());
    EXPECT_EQ(nm.getRequestCount(), 2U);
    nm.userNetworkRelease(0U);
    EXPECT_TRUE(nm.isNetworkRequested());
    nm.userNetworkRelease(1U);
    EXPECT_FALSE(nm.isNetworkRequested());
}

TEST(LinLinkManagerTest, UserRequestIgnoredInNoCommMode)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.setCommMode(linlink::CommMode::NoCommunication);
    nm.userNetworkRequest(0U);
    EXPECT_FALSE(nm.isNetworkRequested());
    EXPECT_EQ(nm.getCommMode(), linlink::CommMode::NoCommunication);
}

TEST(LinLinkManagerTest, CommModeSilentSuppressesTx)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    int txCount = 0;
    nm.setTxCallback([&](uint32_t, uint8_t, uint8_t const*) { ++txCount; return true; });
    nm.setCommMode(linlink::CommMode::SilentCommunication);
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(txCount, 0);
}

TEST(LinLinkManagerTest, PassiveNmNeverTransmits)
{
    linlink::LinLinkManagerConfig c = makeCfg();
    c.activeNm = false;
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(c));
    int txCount = 0;
    nm.setTxCallback([&](uint32_t, uint8_t, uint8_t const*) { ++txCount; return true; });
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(txCount, 0);
}

TEST(LinLinkManagerTest, RemoteWakeupOnActiveWakeupBit)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    bool woken = false;
    nm.setWakeupCallback([&]() { woken = true; });
    // PDU with active wakeup bit set (0x04) from node 2
    uint8_t buf[6] = {0x04, 2, 0, 0, 0, 0};
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    EXPECT_TRUE(woken);
    EXPECT_EQ(nm.getState(), linlink::NmState::RepeatMessage);
}

TEST(LinLinkManagerTest, PartialNetworkingFiltersNonMatchingGroup)
{
    linlink::LinLinkManagerConfig c = makeCfg();
    c.partialNetworkingEnabled = true;
    c.pnGroup = 5U;
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(c));
    // PDU from node 2 with PN group 9 (non-matching), PNI bit set
    uint8_t buf[6] = {0x08, 2, 9, 0, 0, 0}; // cbv PNI, src 2, pnGroup 9
    linlink::NmState before = nm.getState();
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    // should be ignored -> state unchanged (still BusSleep)
    EXPECT_EQ(nm.getState(), before);
    EXPECT_FALSE(nm.isNodeAlive(2U, 50U));
}

TEST(LinLinkManagerTest, PartialNetworkingAcceptsMatchingGroup)
{
    linlink::LinLinkManagerConfig c = makeCfg();
    c.partialNetworkingEnabled = true;
    c.pnGroup = 5U;
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(c));
    uint8_t buf[6] = {0x08, 2, 5, 0, 0, 0};
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    EXPECT_TRUE(nm.isNodeAlive(2U, 50U));
}

TEST(LinLinkManagerTest, BusOffTransitionsToBusSleep)
{
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(nm.getState(), linlink::NmState::RepeatMessage);
    bool cbCalled = false;
    nm.setBusOffCallback([&]() { cbCalled = true; });
    nm.onBusOff();
    EXPECT_TRUE(cbCalled);
    EXPECT_EQ(nm.getState(), linlink::NmState::BusSleep);
}

TEST(LinLinkManagerTest, CoordinatorSetsCSRInReadySleep)
{
    linlink::LinLinkManagerConfig c = makeCfg();
    c.nmCoordinator = true;
    linlink::LinLinkManager nm;
    ASSERT_TRUE(nm.init(c));
    int txCount = 0;
    uint8_t lastCbv = 0;
    nm.setTxCallback([&](uint32_t, uint8_t dlc, uint8_t const* data) {
        ++txCount;
        if (dlc > 0) lastCbv = data[0];
        return true;
    });
    nm.networkRequest();
    nm.mainFunction(0U);
    nm.networkRelease();
    nm.mainFunction(1100U); // ReadySleep
    // CSR bit (0x02) should be set when coordinator ready
    EXPECT_TRUE((lastCbv & 0x02U) != 0U);
}

TEST(LinLinkManagerTest, PniBitInEncodedPdu)
{
    linlink::LinLinkManagerConfig c = makeCfg();
    c.partialNetworkingEnabled = true;
    c.pnGroup = 7U;
    linlink::NmControlBits cb{};
    cb.partialNetworkInfo = true;
    uint8_t ud[4] = {7, 0, 0, 0};
    uint8_t buf[8] = {};
    uint8_t dlc = linlink::LinLinkManager::encodePdu(c, cb, 1U, ud, buf, 8U);
    EXPECT_GE(dlc, 2U);
    EXPECT_TRUE((buf[0] & 0x08U) != 0U);
}

TEST(LinLinkManagerTest, DecodePduWithPnGroup)
{
    uint8_t buf[6] = {0x08, 3, 9, 0, 0, 0};
    linlink::NmControlBits cb{};
    uint8_t src = 0;
    uint8_t ud[8] = {};
    uint8_t len = 0;
    uint8_t pn = 0;
    ASSERT_TRUE(linlink::LinLinkManager::decodePdu(buf, 6, cb, src, ud, len, &pn));
    EXPECT_TRUE(cb.partialNetworkInfo);
    EXPECT_EQ(src, 3U);
    EXPECT_EQ(pn, 9U);
}

TEST(LinLinkManagerTest, ParseNmStateAll)
{
    linlink::NmState s;
    EXPECT_TRUE(linlink::parseNmState("RepeatMessage", s));
    EXPECT_EQ(s, linlink::NmState::RepeatMessage);
    EXPECT_TRUE(linlink::parseNmState("NormalOperation", s));
    EXPECT_TRUE(linlink::parseNmState("ReadySleep", s));
    EXPECT_TRUE(linlink::parseNmState("PrepareBusSleep", s));
}


