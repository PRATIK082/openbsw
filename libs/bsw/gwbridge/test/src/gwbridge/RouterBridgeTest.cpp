/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "gwbridge/RouterBridge.h"

#include "gwbridge/IoBridge.h"

#include <io/MemoryQueue.h>

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

using RawQueue = ::io::MemoryQueue<512U, 32U>;
using TestBridge = ::gwbridge::IoBridge<512U, 32U, 512U, 32U>;

void pushMessage(RawQueue::Writer& writer, uint32_t id, std::vector<uint8_t> const& payload)
{
    ::etl::span<uint8_t> slot = writer.allocate(8U + payload.size());
    ASSERT_EQ(8U + payload.size(), slot.size());
    TestBridge::writeBigEndian32(slot.data(), id);
    TestBridge::writeBigEndian32(slot.data() + 4U, static_cast<uint32_t>(payload.size()));
    for (size_t i = 0U; i < payload.size(); ++i)
    {
        slot[8U + i] = payload[i];
    }
    writer.commit();
}

::gwbridge::BridgeChannelConfig makeChannel(uint8_t index, uint32_t rxId, uint32_t txId)
{
    ::gwbridge::BridgeChannelConfig channel{};
    channel.channelIndex = index;
    ::gwbridge::BridgeRxMessage rx{};
    rx.messageId     = rxId;
    rx.messageLength = 8U;
    ::gwbridge::BridgePduLayout pdu{};
    pdu.offset = 0U;
    pdu.length = 8U;
    rx.pdus.push_back(pdu);
    channel.rxMessages.push_back(rx);
    ::gwbridge::BridgeTxMessage tx{};
    tx.messageId     = txId;
    tx.messageLength = 8U;
    tx.pduOffset     = 0U;
    channel.txMessages.push_back(tx);
    return channel;
}

/**
 * \desc: A PDU received on channel 0 routes to channel 1 with the new id.
 */
TEST(RouterBridgeTest, routes_pdu)
{
    RawQueue rxQueue0, txQueue0, rxQueue1, txQueue1;
    RawQueue::Writer rxWriter0(rxQueue0), txWriter0(txQueue0);
    RawQueue::Writer rxWriter1(rxQueue1), txWriter1(txQueue1);
    RawQueue::Reader rxReader0(rxQueue0), txReader0(txQueue0);
    RawQueue::Reader rxReader1(rxQueue1), txReader1(txQueue1);
    (void)txWriter0;
    (void)rxReader0;
    (void)rxWriter1;
    (void)txReader0;

    ::gwbridge::RouterBridge<2U> bridge;
    ASSERT_TRUE(bridge.addChannel(makeChannel(0U, 0x100U, 0x101U)));
    ASSERT_TRUE(bridge.addChannel(makeChannel(1U, 0x200U, 0x201U)));
    ::gwbridge::BridgeRoute route{};
    route.srcChannel = 0U;
    route.srcMessage = 0x100U;
    route.srcPduIndex = 0U;
    route.dstChannel = 1U;
    route.dstMessage = 0x201U;
    ASSERT_TRUE(bridge.addRoute(route));

    ::io::IReader* readers[2U] = {nullptr, nullptr};
    ::io::IWriter* writers[2U] = {nullptr, nullptr};
    ::io::MemoryQueueReader<RawQueue> rawReader0(rxQueue0), rawReader1(rxQueue1);
    ::io::MemoryQueueWriter<RawQueue> rawWriter0(txQueue0), rawWriter1(txQueue1);
    readers[0] = &rawReader0;
    readers[1] = &rawReader1;
    writers[0] = &rawWriter0;
    writers[1] = &rawWriter1;
    ASSERT_TRUE(bridge.init(::etl::span<::io::IReader*>(readers, 2U),
                            ::etl::span<::io::IWriter*>(writers, 2U)));

    pushMessage(rxWriter0, 0x100U, {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U});
    ASSERT_TRUE(bridge.run());
    EXPECT_EQ(1U, bridge.getRoutedCount());

    ::etl::span<uint8_t> out = txReader1.peek();
    ASSERT_EQ(16U, out.size());
    EXPECT_EQ(0x201U, TestBridge::readBigEndian32(out.data()));
    EXPECT_EQ(8U, TestBridge::readBigEndian32(out.data() + 4U));
    EXPECT_EQ(1U, out[8U]);
    EXPECT_EQ(8U, out[15U]);
    txReader1.release();
}

/**
 * \desc: Unknown message ids count errors and route nothing.
 */
TEST(RouterBridgeTest, unknown_message_errors)
{
    RawQueue rxQueue0, txQueue0, rxQueue1, txQueue1;
    ::io::MemoryQueueReader<RawQueue> rawReader0(rxQueue0), rawReader1(rxQueue1);
    ::io::MemoryQueueWriter<RawQueue> rawWriter0(txQueue0), rawWriter1(txQueue1);
    RawQueue::Writer rxWriter0(rxQueue0);
    RawQueue::Reader txReader1(txQueue1);

    ::gwbridge::RouterBridge<2U> bridge;
    ASSERT_TRUE(bridge.addChannel(makeChannel(0U, 0x100U, 0x101U)));
    ASSERT_TRUE(bridge.addChannel(makeChannel(1U, 0x200U, 0x201U)));
    ::gwbridge::BridgeRoute route{};
    route.srcChannel = 0U;
    route.srcMessage = 0x100U;
    route.dstChannel = 1U;
    route.dstMessage = 0x201U;
    ASSERT_TRUE(bridge.addRoute(route));

    ::io::IReader* readers[2U] = {&rawReader0, &rawReader1};
    ::io::IWriter* writers[2U] = {&rawWriter0, &rawWriter1};
    ASSERT_TRUE(bridge.init(::etl::span<::io::IReader*>(readers, 2U),
                            ::etl::span<::io::IWriter*>(writers, 2U)));

    pushMessage(rxWriter0, 0x999U, {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U});
    EXPECT_FALSE(bridge.run());
    EXPECT_EQ(1U, bridge.getErrorCount());
    EXPECT_TRUE(txReader1.peek().empty());
}

/**
 * \desc: Bad configurations are rejected at init, not at runtime.
 */
TEST(RouterBridgeTest, rejects_bad_config)
{
    ::gwbridge::RouterBridge<2U> bridge;
    ::gwbridge::BridgeChannelConfig dup = makeChannel(0U, 0x100U, 0x101U);
    ASSERT_TRUE(bridge.addChannel(dup));
    EXPECT_FALSE(bridge.addChannel(dup)); // duplicate channel index

    ::gwbridge::BridgeChannelConfig outOfRange = makeChannel(9U, 0x100U, 0x101U);
    EXPECT_FALSE(bridge.addChannel(outOfRange));

    ::gwbridge::BridgeRoute badRoute{};
    badRoute.srcChannel = 0U;
    badRoute.srcMessage = 0x999U; // unknown message
    badRoute.dstChannel = 1U;     // unknown channel
    badRoute.dstMessage = 0x201U;
    ASSERT_TRUE(bridge.addRoute(badRoute));

    RawQueue rxQueue0, txQueue0;
    ::io::MemoryQueueReader<RawQueue> rawReader0(rxQueue0);
    ::io::MemoryQueueWriter<RawQueue> rawWriter0(txQueue0);
    ::io::IReader* readers[2U] = {&rawReader0, nullptr};
    ::io::IWriter* writers[2U] = {&rawWriter0, nullptr};
    EXPECT_FALSE(bridge.init(::etl::span<::io::IReader*>(readers, 2U),
                             ::etl::span<::io::IWriter*>(writers, 2U)));
    EXPECT_FALSE(bridge.isInitialized());
}

} // namespace
