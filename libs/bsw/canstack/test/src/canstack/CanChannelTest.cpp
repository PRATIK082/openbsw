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
#include "canstack/CanChannel.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

/**
 * \desc: init() rejects null drivers and mailbox counts of zero.
 */
TEST(CanChannelTest, init_rejects_invalid_config)
{
    ::canstack::CanChannel channel;
    ::canstack::CanChannelConfig config{};
    config.channelId   = 0U;
    config.hwDriver     = nullptr;
    config.baudrate    = 500000U;
    config.maxMailboxes = 0U;

    EXPECT_FALSE(channel.init(config));
    EXPECT_FALSE(channel.isInitialized());
}

/**
 * \desc: init() creates the configured mailboxes and brings the driver up.
 */
TEST(CanChannelTest, init_creates_mailboxes)
{
    ::canstack::testutils::LoopbackChannel setup(0U);
    ::canstack::CanChannel& channel = setup.channel;

    EXPECT_TRUE(channel.isInitialized());
    EXPECT_EQ(0U, channel.getChannelId());
    ASSERT_EQ(2U, channel.getMailboxes().size());
    EXPECT_TRUE(channel.getMailboxes()[0].acceptAll);
    EXPECT_EQ(&setup.hw, channel.getHwDriver());
}

/**
 * \desc: Frames injected at the driver are forwarded to the upper layer with
 * the channel id attached.
 */
TEST(CanChannelTest, forwards_rx_with_channel_id)
{
    ::canstack::testutils::LoopbackChannel setup(5U);
    ::canstack::testutils::FrameSink sink;
    sink.connect(setup.channel);

    uint8_t const data[2] = {0x01U, 0x02U};
    setup.hw.injectRxFrame(0x150U, 2U, data);
    setup.channel.mainFunction();

    ASSERT_EQ(1U, sink.entries().size());
    EXPECT_EQ(5U, sink.entries()[0].channelId);
    EXPECT_EQ(0x150U, sink.entries()[0].frameId);
    EXPECT_THAT(sink.entries()[0].data, ElementsAre(0x01U, 0x02U));
}

/**
 * \desc: transmitFrame() sends through the driver and cycles the tx mailboxes.
 */
TEST(CanChannelTest, transmit_cycles_mailboxes)
{
    ::canstack::CanHwStub hw; // no loopback: only tx logging
    ::canstack::CanChannel channel;
    ::canstack::CanChannelConfig config{};
    config.channelId   = 0U;
    config.hwDriver     = &hw;
    config.baudrate    = 500000U;
    config.maxMailboxes = 3U;
    (void)channel.init(config);

    uint8_t const data[1] = {0U};
    EXPECT_TRUE(channel.transmitFrame(0x10U, 1U, data));
    EXPECT_TRUE(channel.transmitFrame(0x11U, 1U, data));
    EXPECT_TRUE(channel.transmitFrame(0x12U, 1U, data));

    // The stub ignores mailboxes, but the round robin counter must not leave
    // the configured range: 3 frames, 3 mailboxes, next is 0 again.
    EXPECT_TRUE(channel.transmitFrame(0x13U, 1U, data));
    ASSERT_EQ(4U, hw.getTxLog().size());
    EXPECT_EQ(0x10U, hw.getTxLog()[0].getFrameId());
    EXPECT_EQ(0x13U, hw.getTxLog()[3].getFrameId());
}

/**
 * \desc: transmitFrame() before init fails.
 */
TEST(CanChannelTest, transmit_without_init_fails)
{
    ::canstack::CanChannel channel;
    uint8_t const data[1] = {0U};

    EXPECT_FALSE(channel.transmitFrame(0x10U, 1U, data));
}

/**
 * \desc: shutdown() releases the driver and resets the channel.
 */
TEST(CanChannelTest, shutdown_releases_driver)
{
    ::canstack::testutils::LoopbackChannel setup(0U);

    setup.channel.shutdown();

    EXPECT_FALSE(setup.channel.isInitialized());
    EXPECT_FALSE(setup.hw.isInitialized());
    EXPECT_TRUE(setup.channel.getMailboxes().empty());
    EXPECT_EQ(nullptr, setup.channel.getHwDriver());
}

} // namespace
