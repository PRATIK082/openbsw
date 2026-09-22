/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "gwbridge/BridgeSupervisor.h"
#include "gwbridge/CanIoBridge.h"

#include "canstack/CanChannel.h"
#include "canstack/CanHwStub.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

class TestSupervisor : public ::gwbridge::BridgeSupervisor
{
public:
    void publicInit() { init(); }
    void publicShutdown() { shutdown(); }
};

struct CanPair
{
    ::canstack::CanHwStub hw;
    ::canstack::CanChannel channel;
    ::gwbridge::CanIoBridge bridge;

    explicit CanPair(uint8_t channelId)
    {
        hw.setLoopback(false);
        ::canstack::CanChannelConfig config{};
        config.channelId    = channelId;
        config.hwDriver     = &hw;
        config.baudrate     = 500000U;
        config.maxMailboxes = 2U;
        (void)channel.init(config);
        bridge.bind(channel);
    }
};

::gwbridge::BridgeChannelConfig makeCanChannel(uint8_t index, uint32_t rxId, uint32_t txId)
{
    ::gwbridge::BridgeChannelConfig channel{};
    channel.channelIndex = index;
    ::gwbridge::BridgeRxMessage rx{};
    rx.messageId     = rxId;
    rx.messageLength = 8U;
    ::gwbridge::BridgePduLayout pdu{};
    pdu.length = 8U;
    rx.pdus.push_back(pdu);
    channel.rxMessages.push_back(rx);
    ::gwbridge::BridgeTxMessage tx{};
    tx.messageId     = txId;
    tx.messageLength = 8U;
    channel.txMessages.push_back(tx);
    return channel;
}

/**
 * \desc: CAN ch0 -> Router -> CAN ch1 end to end through the supervisor.
 */
TEST(BridgeSupervisorTest, end_to_end)
{
    CanPair bus0(0U);
    CanPair bus1(1U);

    ::gwbridge::SupervisorChannel ch0{};
    ch0.config = makeCanChannel(0U, 0x100U, 0x101U);
    ch0.reader = &bus0.bridge.rxReader();
    ch0.writer = &bus0.bridge.txWriter();
    ch0.pumpTx  = [&bus0]() { return bus0.bridge.pumpTx(bus0.channel); };
    ::gwbridge::SupervisorChannel ch1{};
    ch1.config = makeCanChannel(1U, 0x200U, 0x201U);
    ch1.reader = &bus1.bridge.rxReader();
    ch1.writer = &bus1.bridge.txWriter();
    ch1.pumpTx  = [&bus1]() { return bus1.bridge.pumpTx(bus1.channel); };

    ::gwbridge::BridgeRoute route{};
    route.srcChannel = 0U;
    route.srcMessage = 0x100U;
    route.dstChannel = 1U;
    route.dstMessage = 0x201U;

    ::gwbridge::SupervisorConfig config{};
    config.channels.push_back(ch0);
    config.channels.push_back(ch1);
    config.routes.push_back(route);

    auto& supervisor = ::gwbridge::BridgeSupervisor::getInstance();
    supervisor.configure(config, ::async::CONTEXT_INVALID);
    ASSERT_TRUE(supervisor.isConfigured());
    static_cast<TestSupervisor&>(supervisor).publicInit();
    ASSERT_TRUE(supervisor.getRouterBridge().isInitialized());

    uint8_t const payload[8U] = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
    bus0.hw.injectRxFrame(0x100U, 8U, payload);
    bus0.channel.mainFunction();
    supervisor.pumpOnce();

    ASSERT_EQ(1U, bus1.hw.getTxLog().size());
    EXPECT_EQ(0x201U, bus1.hw.getTxLog()[0].getFrameId());
    EXPECT_EQ(8U, bus1.hw.getTxLog()[0].getDlc());
    uint8_t const* routed = bus1.hw.getTxLog()[0].getData();
    for (uint8_t i = 0U; i < 8U; ++i)
    {
        EXPECT_EQ(payload[i], routed[i]);
    }
    EXPECT_EQ(1U, supervisor.getRoutedCount());

    static_cast<TestSupervisor&>(supervisor).publicShutdown();
}

} // namespace
