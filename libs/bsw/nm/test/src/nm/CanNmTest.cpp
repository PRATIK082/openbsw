/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <nm/CanNm.h>
#include <nm/GatewayNm.h>

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace
{

nm::CanNmConfig makeCfg(uint8_t nodeId = 1U)
{
    nm::CanNmConfig c{};
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

TEST(CanNmTest, InitRejectsInvalidConfig)
{
    nm::CanNm nm;
    nm::CanNmConfig c = makeCfg();
    c.maxNodes = 0U;
    EXPECT_FALSE(nm.init(c));
    c = makeCfg();
    c.tTx = 0U;
    EXPECT_FALSE(nm.init(c));
    c = makeCfg();
    c.userDataLength = 7U;
    EXPECT_FALSE(nm.init(c));
}

TEST(CanNmTest, EncodeDecodeRoundTrip)
{
    nm::CanNmConfig c = makeCfg(3U);
    nm::NmControlBits cb{};
    cb.repeatMessageRequest = true;
    cb.activeWakeup = true;
    uint8_t ud[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t buf[8] = {};
    uint8_t dlc = nm::CanNm::encodePdu(c, cb, 3U, ud, buf, 8U);
    EXPECT_EQ(dlc, 6U); // 2 + 4

    nm::NmControlBits cb2{};
    uint8_t src = 0U;
    uint8_t ud2[8] = {};
    uint8_t len = 0U;
    ASSERT_TRUE(nm::CanNm::decodePdu(buf, dlc, cb2, src, ud2, len));
    EXPECT_TRUE(cb2.repeatMessageRequest);
    EXPECT_FALSE(cb2.nmCoordinatorSleepReady);
    EXPECT_TRUE(cb2.activeWakeup);
    EXPECT_EQ(src, 3U);
    EXPECT_EQ(len, 4U);
    EXPECT_EQ(std::memcmp(ud, ud2, 4), 0);
}

TEST(CanNmTest, DecodeRejectsTooSmall)
{
    uint8_t buf[1] = {0};
    nm::NmControlBits cb{};
    uint8_t src = 0;
    uint8_t ud[8] = {};
    uint8_t len = 0;
    EXPECT_FALSE(nm::CanNm::decodePdu(buf, 1, cb, src, ud, len));
    EXPECT_FALSE(nm::CanNm::decodePdu(nullptr, 2, cb, src, ud, len));
}

TEST(CanNmTest, InitialStateBusSleep)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    EXPECT_EQ(nm.getState(), nm::NmState::BusSleep);
    EXPECT_FALSE(nm.isNetworkRequested());
}

TEST(CanNmTest, NetworkRequestWakesToRepeatMessage)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    bool woken = false;
    nm.setWakeupCallback([&]() { woken = true; });
    nm.networkRequest();
    EXPECT_TRUE(nm.isNetworkRequested());
    nm.mainFunction(0U);
    EXPECT_EQ(nm.getState(), nm::NmState::RepeatMessage);
    EXPECT_TRUE(woken);
}

TEST(CanNmTest, NetworkReleaseEventuallySleeps)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(nm.getState(), nm::NmState::RepeatMessage);

    nm.networkRelease();
    EXPECT_FALSE(nm.isNetworkRequested());
    // stay in repeat for tRepeatMessage
    nm.mainFunction(500U);
    // after tRepeatMessage (1000ms) with no request -> ReadySleep
    nm.mainFunction(1100U);
    EXPECT_EQ(nm.getState(), nm::NmState::ReadySleep);
}

TEST(CanNmTest, ReceivedPduWakesFromBusSleep)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    EXPECT_EQ(nm.getState(), nm::NmState::BusSleep);

    uint8_t buf[6] = {0, 2, 0, 0, 0, 0}; // node 2, empty ud
    nm.onNmPduReceived(2U, buf, 6U, 100U);
    EXPECT_EQ(nm.getState(), nm::NmState::RepeatMessage);
}

TEST(CanNmTest, NodeTrackingAliveAndDead)
{
    nm::CanNm nm;
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

TEST(CanNmTest, AliveNodeCount)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg(1U)));
    nm.networkRequest();
    nm.mainFunction(0U); // self active in RepeatMessage
    EXPECT_GE(nm.getAliveNodeCount(), 1U);

    uint8_t buf[6] = {0, 2, 0, 0, 0, 0};
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    EXPECT_GE(nm.getAliveNodeCount(), 2U);
}

TEST(CanNmTest, ReadySleepToPrepareBusSleepWhenAlone)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    nm.networkRelease();
    nm.mainFunction(1100U); // -> ReadySleep
    EXPECT_EQ(nm.getState(), nm::NmState::ReadySleep);
    // no other nodes, tNm expired relative to last rx (0) -> PrepareBusSleep
    nm.mainFunction(5000U);
    EXPECT_EQ(nm.getState(), nm::NmState::PrepareBusSleep);
}

TEST(CanNmTest, PrepareBusSleepTimesOutToBusSleep)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    nm.networkRelease();
    nm.mainFunction(1100U); // ReadySleep
    nm.mainFunction(5000U); // PrepareBusSleep
    EXPECT_EQ(nm.getState(), nm::NmState::PrepareBusSleep);
    bool slept = false;
    nm.setSleepCallback([&]() { slept = true; });
    nm.mainFunction(5000U + 1600U);
    EXPECT_EQ(nm.getState(), nm::NmState::BusSleep);
    EXPECT_TRUE(slept);
}

TEST(CanNmTest, TxCallbackInvokedInRepeatMessage)
{
    nm::CanNm nm;
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

TEST(CanNmTest, ParseNmState)
{
    nm::NmState s;
    EXPECT_TRUE(nm::parseNmState("BusSleep", s));
    EXPECT_EQ(s, nm::NmState::BusSleep);
    EXPECT_FALSE(nm::parseNmState("Nope", s));
    EXPECT_FALSE(nm::parseNmState(nullptr, s));
}

TEST(CanNmTest, UserDataLocal)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    uint8_t in[4] = {1, 2, 3, 4};
    nm.setUserData(in, 4);
    uint8_t out[4] = {};
    nm.getUserData(out, 4);
    EXPECT_EQ(std::memcmp(in, out, 4), 0);
}

TEST(CanNmTest, UserManagementAggregation)
{
    nm::CanNm nm;
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

TEST(CanNmTest, UserRequestIgnoredInNoCommMode)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.setCommMode(nm::CommMode::NoCommunication);
    nm.userNetworkRequest(0U);
    EXPECT_FALSE(nm.isNetworkRequested());
    EXPECT_EQ(nm.getCommMode(), nm::CommMode::NoCommunication);
}

TEST(CanNmTest, CommModeSilentSuppressesTx)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    int txCount = 0;
    nm.setTxCallback([&](uint32_t, uint8_t, uint8_t const*) { ++txCount; return true; });
    nm.setCommMode(nm::CommMode::SilentCommunication);
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(txCount, 0);
}

TEST(CanNmTest, PassiveNmNeverTransmits)
{
    nm::CanNmConfig c = makeCfg();
    c.activeNm = false;
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(c));
    int txCount = 0;
    nm.setTxCallback([&](uint32_t, uint8_t, uint8_t const*) { ++txCount; return true; });
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(txCount, 0);
}

TEST(CanNmTest, RemoteWakeupOnActiveWakeupBit)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    bool woken = false;
    nm.setWakeupCallback([&]() { woken = true; });
    // PDU with active wakeup bit set (0x04) from node 2
    uint8_t buf[6] = {0x04, 2, 0, 0, 0, 0};
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    EXPECT_TRUE(woken);
    EXPECT_EQ(nm.getState(), nm::NmState::RepeatMessage);
}

TEST(CanNmTest, PartialNetworkingFiltersNonMatchingGroup)
{
    nm::CanNmConfig c = makeCfg();
    c.partialNetworkingEnabled = true;
    c.pnGroup = 5U;
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(c));
    // PDU from node 2 with PN group 9 (non-matching), PNI bit set
    uint8_t buf[6] = {0x08, 2, 9, 0, 0, 0}; // cbv PNI, src 2, pnGroup 9
    nm::NmState before = nm.getState();
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    // should be ignored -> state unchanged (still BusSleep)
    EXPECT_EQ(nm.getState(), before);
    EXPECT_FALSE(nm.isNodeAlive(2U, 50U));
}

TEST(CanNmTest, PartialNetworkingAcceptsMatchingGroup)
{
    nm::CanNmConfig c = makeCfg();
    c.partialNetworkingEnabled = true;
    c.pnGroup = 5U;
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(c));
    uint8_t buf[6] = {0x08, 2, 5, 0, 0, 0};
    nm.onNmPduReceived(2U, buf, 6U, 50U);
    EXPECT_TRUE(nm.isNodeAlive(2U, 50U));
}

TEST(CanNmTest, BusOffTransitionsToBusSleep)
{
    nm::CanNm nm;
    ASSERT_TRUE(nm.init(makeCfg()));
    nm.networkRequest();
    nm.mainFunction(0U);
    EXPECT_EQ(nm.getState(), nm::NmState::RepeatMessage);
    bool cbCalled = false;
    nm.setBusOffCallback([&]() { cbCalled = true; });
    nm.onBusOff();
    EXPECT_TRUE(cbCalled);
    EXPECT_EQ(nm.getState(), nm::NmState::BusSleep);
}

TEST(CanNmTest, CoordinatorSetsCSRInReadySleep)
{
    nm::CanNmConfig c = makeCfg();
    c.nmCoordinator = true;
    nm::CanNm nm;
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

TEST(CanNmTest, PniBitInEncodedPdu)
{
    nm::CanNmConfig c = makeCfg();
    c.partialNetworkingEnabled = true;
    c.pnGroup = 7U;
    nm::NmControlBits cb{};
    cb.partialNetworkInfo = true;
    uint8_t ud[4] = {7, 0, 0, 0};
    uint8_t buf[8] = {};
    uint8_t dlc = nm::CanNm::encodePdu(c, cb, 1U, ud, buf, 8U);
    EXPECT_GE(dlc, 2U);
    EXPECT_TRUE((buf[0] & 0x08U) != 0U);
}

TEST(CanNmTest, DecodePduWithPnGroup)
{
    uint8_t buf[6] = {0x08, 3, 9, 0, 0, 0};
    nm::NmControlBits cb{};
    uint8_t src = 0;
    uint8_t ud[8] = {};
    uint8_t len = 0;
    uint8_t pn = 0;
    ASSERT_TRUE(nm::CanNm::decodePdu(buf, 6, cb, src, ud, len, &pn));
    EXPECT_TRUE(cb.partialNetworkInfo);
    EXPECT_EQ(src, 3U);
    EXPECT_EQ(pn, 9U);
}

TEST(CanNmTest, ParseNmStateAll)
{
    nm::NmState s;
    EXPECT_TRUE(nm::parseNmState("RepeatMessage", s));
    EXPECT_EQ(s, nm::NmState::RepeatMessage);
    EXPECT_TRUE(nm::parseNmState("NormalOperation", s));
    EXPECT_TRUE(nm::parseNmState("ReadySleep", s));
    EXPECT_TRUE(nm::parseNmState("PrepareBusSleep", s));
}

TEST(GatewayNmTest, RegisterAndCountChannels)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    EXPECT_TRUE(gw.registerChannel(&a));
    EXPECT_TRUE(gw.registerChannel(&b));
    EXPECT_FALSE(gw.registerChannel(&a)); // duplicate
    EXPECT_FALSE(gw.registerChannel(nullptr));
    EXPECT_EQ(gw.getChannelCount(), 2U);
}

TEST(GatewayNmTest, RequestOnOneChannelWakesAll)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    gw.registerChannel(&a);
    gw.registerChannel(&b);

    bool woke = false;
    gw.setGlobalWakeupCallback([&]() { woke = true; });
    gw.onChannelNetworkRequest(0U);
    EXPECT_TRUE(a.isNetworkRequested());
    EXPECT_TRUE(b.isNetworkRequested());
    EXPECT_TRUE(woke);
}

TEST(GatewayNmTest, AllReadyToSleepOnlyWhenAllReady)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    gw.registerChannel(&a);
    gw.registerChannel(&b);

    // initially both BusSleep -> not all ready? BusSleep counts as ready-to-sleep
    EXPECT_TRUE(gw.areAllChannelsReadyToSleep());

    a.networkRequest();
    a.mainFunction(0U);
    EXPECT_FALSE(gw.areAllChannelsReadyToSleep());
}

TEST(GatewayNmTest, GlobalSleepCallbackWhenAllReady)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);

    a.networkRequest();
    gw.mainFunction(0U);
    a.networkRelease();
    gw.mainFunction(1100U); // ReadySleep

    bool slept = false;
    gw.setGlobalSleepCallback([&]() { slept = true; });
    gw.mainFunction(5000U);
    EXPECT_TRUE(slept);
}

TEST(GatewayNmTest, MainFunctionDrivesChannels)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);
    gw.mainFunction(0U);
    EXPECT_EQ(a.getState(), nm::NmState::BusSleep);
}

TEST(GatewayNmTest, ParseChannelSleepModeAndEcuState)
{
    nm::ChannelSleepMode m;
    EXPECT_TRUE(nm::parseChannelSleepMode("KeepAwake", m));
    EXPECT_EQ(m, nm::ChannelSleepMode::KeepAwake);
    nm::EcuState s;
    EXPECT_TRUE(nm::parseEcuState("PrepareSleep", s));
    EXPECT_EQ(s, nm::EcuState::PrepareSleep);
}

TEST(GatewayNmTest, PerChannelSleepModeIndependent)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    gw.registerChannel(&a);
    gw.registerChannel(&b);

    // put channel 1 to sleep independently while ECU stays awake
    ASSERT_TRUE(gw.setChannelSleepMode(1U, nm::ChannelSleepMode::Sleep));
    EXPECT_EQ(gw.getChannelSleepMode(1U), nm::ChannelSleepMode::Sleep);
    EXPECT_EQ(gw.getChannelSleepMode(0U), nm::ChannelSleepMode::Awake);

    // channel 0 requests network
    a.networkRequest();
    gw.mainFunction(0U);
    EXPECT_TRUE(a.isNetworkRequested());
    // sleeping channel should not be requested
    EXPECT_FALSE(b.isNetworkRequested());
}

TEST(GatewayNmTest, KeepAwakeHoldsEcuAwake)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);

    gw.channelKeepAwake(0U, true);
    EXPECT_TRUE(gw.isChannelKeptAwake(0U));
    EXPECT_TRUE(gw.isNetworkRequested());

    // even with no NM request, ECU stays awake
    gw.mainFunction(0U);
    EXPECT_TRUE(gw.isEcuAwake());
}

TEST(GatewayNmTest, ChannelWakeupFromSleep)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    gw.registerChannel(&a);
    gw.registerChannel(&b);

    // drive to sleep
    a.networkRequest(); b.networkRequest();
    gw.mainFunction(0U);
    a.networkRelease(); b.networkRelease();
    gw.mainFunction(1100U);
    gw.mainFunction(5000U); // sleep
    EXPECT_EQ(gw.getEcuState(), nm::EcuState::Sleep);

    // wakeup on channel 0
    gw.channelWakeup(0U);
    EXPECT_EQ(gw.getEcuState(), nm::EcuState::Run);
    EXPECT_TRUE(a.isNetworkRequested());
}

TEST(GatewayNmTest, RequiredSleepChannelsSleepOnEcuSleep)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    gw.registerChannel(&a);
    gw.registerChannel(&b);
    // channel 1 is NOT required to sleep (stays awake for diagnostics)
    gw.setChannelRequiredSleep(1U, false);

    a.networkRequest(); b.networkRequest();
    gw.mainFunction(0U);
    a.networkRelease(); b.networkRelease();
    gw.mainFunction(1100U);
    gw.mainFunction(5000U);
    EXPECT_EQ(gw.getEcuState(), nm::EcuState::Sleep);
    // required channel 0 should be in sleep mode
    EXPECT_EQ(gw.getChannelSleepMode(0U), nm::ChannelSleepMode::Sleep);
    // non-required channel 1 should remain awake mode
    EXPECT_EQ(gw.getChannelSleepMode(1U), nm::ChannelSleepMode::Awake);
}

TEST(GatewayNmTest, KeepAwakeBlocksGlobalSleep)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);

    a.networkRequest();
    gw.mainFunction(0U);
    gw.channelKeepAwake(0U, true);
    a.networkRelease();
    gw.mainFunction(1100U);
    gw.mainFunction(5000U);
    // keep-awake holds ECU awake
    EXPECT_TRUE(gw.isEcuAwake());
    EXPECT_FALSE(gw.areAllChannelsReadyToSleep());
}

TEST(GatewayNmTest, ChannelSleepModeSilentSetsCommMode)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);
    ASSERT_TRUE(gw.setChannelSleepMode(0U, nm::ChannelSleepMode::Silent));
    EXPECT_EQ(a.getCommMode(), nm::CommMode::SilentCommunication);
}

TEST(GatewayNmTest, AwakeChannelCount)
{
    nm::GatewayNm gw;
    nm::CanNm a, b;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    ASSERT_TRUE(b.init(c));
    gw.registerChannel(&a);
    gw.registerChannel(&b);
    EXPECT_EQ(gw.getAwakeChannelCount(), 2U);
    gw.channelSleep(1U);
    EXPECT_EQ(gw.getAwakeChannelCount(), 1U);
}

TEST(GatewayNmTest, ChannelInfoAccess)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);
    auto const* ci = gw.getChannel(0U);
    ASSERT_NE(ci, nullptr);
    EXPECT_EQ(ci->nm, &a);
    EXPECT_EQ(gw.getChannel(99U), nullptr);
}

TEST(GatewayNmTest, SetChannelRequiredSleepValidation)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);
    EXPECT_FALSE(gw.setChannelRequiredSleep(5U, false));
    EXPECT_TRUE(gw.setChannelRequiredSleep(0U, false));
}

TEST(GatewayNmTest, EcuStateOffToStartupOnRegister)
{
    nm::GatewayNm gw;
    nm::CanNm a;
    nm::CanNmConfig c = makeCfg();
    ASSERT_TRUE(a.init(c));
    gw.registerChannel(&a);
    // state becomes Startup on register; first mainFunction moves to Run
    gw.mainFunction(0U);
    EXPECT_EQ(gw.getEcuState(), nm::EcuState::Run);
}
