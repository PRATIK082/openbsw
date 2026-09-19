/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "canstack/TestUtils.h"
#include "canstack/CanInterface.h"

#include <gmock/gmock.h>

#include <vector>

namespace
{
using namespace ::testing;

/**
 * \desc: The stack builds all channels from the code-first configuration.
 */
TEST(CanInterfaceTest, init_from_code_first_config)
{
    ::canstack::CanInterface& stack = ::canstack::CanInterface::getInstance();

    ::canstack::CanHwStub hw0;
    ::canstack::CanHwStub hw1;
    ::canstack::CanChannelConfig const channels[]
        = {{0U, &hw0, 500000U, 2U}, {1U, &hw1, 250000U, 1U}};

    ::canstack::CanStackConfig config{};
    config.channelConfigs = channels;
    config.channelCount    = 2U;
    config.frameTable      = ::canstack::testutils::DEMO_FRAMES;
    config.frameCount      = ::canstack::testutils::DEMO_FRAME_COUNT;
    config.signalTable     = ::canstack::testutils::DEMO_SIGNALS;
    config.signalCount     = ::canstack::testutils::DEMO_SIGNAL_COUNT;

    EXPECT_TRUE(stack.init(config));
    EXPECT_TRUE(stack.isInitialized());
    EXPECT_EQ(2U, stack.getChannelCount());
    EXPECT_EQ(3U, stack.getSignalDatabase().getFrameCount());

    // Channels are addressable by id.
    EXPECT_NE(nullptr, stack.getChannel(0U));
    EXPECT_NE(nullptr, stack.getChannel(1U));
    EXPECT_EQ(nullptr, stack.getChannel(9U));

    stack.shutdown();
    EXPECT_FALSE(stack.isInitialized());
    EXPECT_EQ(0U, stack.getChannelCount());
}

/**
 * \desc: sendFrame() writes through the channel's driver.
 */
TEST(CanInterfaceTest, send_frame_via_channel)
{
    ::canstack::CanInterface& stack = ::canstack::CanInterface::getInstance();

    ::canstack::CanHwStub hw;
    ::canstack::CanChannelConfig const channels[] = {{2U, &hw, 500000U, 2U}};

    ::canstack::CanStackConfig config{};
    config.channelConfigs = channels;
    config.channelCount    = 1U;
    ASSERT_TRUE(stack.init(config));

    uint8_t const data[2] = {0xCAU, 0xFEU};
    EXPECT_TRUE(stack.sendFrame(2U, 0x123U, 2U, data));
    EXPECT_FALSE(stack.sendFrame(7U, 0x123U, 2U, data)); // unknown channel

    ASSERT_EQ(1U, hw.getTxLog().size());
    EXPECT_EQ(0x123U, hw.getTxLog()[0].getFrameId());

    stack.shutdown();
}

/**
 * \desc: Received frames update readSignal() and fire registered callbacks
 * with the unpacked values.
 */
TEST(CanInterfaceTest, receive_path_updates_signals_and_callbacks)
{
    ::canstack::CanInterface& stack = ::canstack::CanInterface::getInstance();

    ::canstack::CanHwStub hw;
    ::canstack::CanChannelConfig const channels[] = {{0U, &hw, 500000U, 2U}};

    ::canstack::CanStackConfig config{};
    config.channelConfigs = channels;
    config.channelCount    = 1U;
    config.frameTable      = ::canstack::testutils::DEMO_FRAMES;
    config.frameCount      = ::canstack::testutils::DEMO_FRAME_COUNT;
    config.signalTable     = ::canstack::testutils::DEMO_SIGNALS;
    config.signalCount     = ::canstack::testutils::DEMO_SIGNAL_COUNT;
    ASSERT_TRUE(stack.init(config));

    double receivedSpeed = -1.0;
    stack.registerSignalCallback(
        "VehicleSpeed", [&receivedSpeed](double value) { receivedSpeed = value; });

    // Build an EngineData frame: EngineSpeed = 1000 rpm, VehicleSpeed = 100 kph.
    uint8_t data[8] = {};
    ::canstack::SignalDatabase const& db = stack.getSignalDatabase();
    db.packSignal(data, *db.getSignalByName("EngineSpeed"), 1000.0);
    db.packSignal(data, *db.getSignalByName("VehicleSpeed"), 100.0);

    hw.injectRxFrame(0x100U, 8U, data);
    stack.mainFunctionRx();

    EXPECT_DOUBLE_EQ(1000.0, stack.readSignal("EngineSpeed"));
    EXPECT_DOUBLE_EQ(100.0, stack.readSignal("VehicleSpeed"));
    EXPECT_DOUBLE_EQ(0.0, stack.readSignal("UnknownSignal")); // unknown signals read as 0
    EXPECT_DOUBLE_EQ(100.0, receivedSpeed);

    stack.shutdown();
}

/**
 * \desc: sendSignal() feeds the cyclic scheduler of registered signals and
 * rejects unknown signals.
 */
TEST(CanInterfaceTest, send_signal_feeds_scheduler)
{
    ::canstack::CanInterface& stack = ::canstack::CanInterface::getInstance();

    ::canstack::CanHwStub hw;
    hw.setLoopback(true);
    ::canstack::CanChannelConfig const channels[] = {{0U, &hw, 500000U, 2U}};

    ::canstack::CanStackConfig config{};
    config.channelConfigs = channels;
    config.channelCount    = 1U;
    config.frameTable      = ::canstack::testutils::DEMO_FRAMES;
    config.frameCount      = ::canstack::testutils::DEMO_FRAME_COUNT;
    config.signalTable     = ::canstack::testutils::DEMO_SIGNALS;
    config.signalCount     = ::canstack::testutils::DEMO_SIGNAL_COUNT;
    ASSERT_TRUE(stack.init(config));

    ::canstack::TxSignalEntry entry;
    entry.signalName  = "TargetSpeed";
    entry.frameId     = 0x200U;
    entry.channelId   = 0U;
    entry.cycleTimeMs = 5U;
    entry.valueProvider = []() { return 300.0; };
    stack.getTxScheduler().registerSignal(entry);

    EXPECT_FALSE(stack.sendSignal("NotRegistered", 1.0));
    EXPECT_TRUE(stack.sendSignal("TargetSpeed", 400.0));

    // 6 ticks: due at t=5.
    for (uint32_t i = 0U; i < 6U; ++i)
    {
        stack.mainFunctionTx();
    }

    ASSERT_EQ(1U, hw.getTxLog().size());
    EXPECT_EQ(0x200U, hw.getTxLog()[0].getFrameId());

    // The loopback delivers the frame back to the receive path.
    stack.mainFunctionRx();
    EXPECT_DOUBLE_EQ(300.0, stack.readSignal("TargetSpeed"));

    stack.shutdown();
}

/**
 * \desc: The value provider wins over the pending value from sendSignal().
 */
TEST(CanInterfaceTest, value_provider_wins)
{
    ::canstack::CanInterface& stack = ::canstack::CanInterface::getInstance();

    ::canstack::CanHwStub hw;
    hw.setLoopback(true);
    ::canstack::CanChannelConfig const channels[] = {{0U, &hw, 500000U, 2U}};

    ::canstack::CanStackConfig config{};
    config.channelConfigs = channels;
    config.channelCount    = 1U;
    config.signalTable     = ::canstack::testutils::DEMO_SIGNALS;
    config.signalCount     = ::canstack::testutils::DEMO_SIGNAL_COUNT;
    config.frameTable      = ::canstack::testutils::DEMO_FRAMES;
    config.frameCount      = ::canstack::testutils::DEMO_FRAME_COUNT;
    ASSERT_TRUE(stack.init(config));

    ::canstack::TxSignalEntry entry;
    entry.signalName    = "TargetSpeed";
    entry.frameId       = 0x200U;
    entry.channelId     = 0U;
    entry.cycleTimeMs   = 5U;
    entry.valueProvider = []() { return 600.0; };
    stack.getTxScheduler().registerSignal(entry);

    (void)stack.sendSignal("TargetSpeed", 400.0); // pending, but provider wins

    for (uint32_t i = 0U; i < 6U; ++i)
    {
        stack.mainFunctionTx();
    }

    stack.mainFunctionRx();
    EXPECT_DOUBLE_EQ(600.0, stack.readSignal("TargetSpeed"));

    stack.shutdown();
}

} // namespace
