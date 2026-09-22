/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "gwbridge/IoBridge.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

using TestBridge = ::gwbridge::IoBridge<256U, 32U, 256U, 32U>;

/**
 * \desc: Pushed frames come out of rxReader with a BE id/length header.
 */
TEST(IoBridgeTest, push_and_read)
{
    TestBridge bridge;
    uint8_t const payload[3U] = {0xAAU, 0xBBU, 0xCCU};
    ASSERT_TRUE(bridge.pushRxFrame(0x100U, payload, 3U));

    ::etl::span<uint8_t> message = bridge.rxReader().peek();
    ASSERT_EQ(11U, message.size());
    EXPECT_EQ(0x100U, TestBridge::readBigEndian32(message.data()));
    EXPECT_EQ(3U, TestBridge::readBigEndian32(message.data() + 4U));
    EXPECT_THAT(std::vector<uint8_t>(message.data() + 8U, message.data() + 11U),
                ElementsAre(0xAAU, 0xBBU, 0xCCU));
    bridge.rxReader().release();
    EXPECT_TRUE(bridge.rxReader().peek().empty());
}

/**
 * \desc: Oversize frames are rejected and counted, not truncated.
 */
TEST(IoBridgeTest, oversize_drops)
{
    TestBridge bridge;
    uint8_t payload[64U] = {0U};
    EXPECT_FALSE(bridge.pushRxFrame(0x100U, payload, 64U)); // 8 + 64 > 32 element
    EXPECT_EQ(1U, bridge.getRxDropCount());
    EXPECT_TRUE(bridge.rxReader().peek().empty());
}

/**
 * \desc: Committed TX messages drain into the sink with parsed headers.
 */
TEST(IoBridgeTest, drain_tx)
{
    TestBridge bridge;
    ::etl::span<uint8_t> slot = bridge.txWriter().allocate(10U);
    ASSERT_EQ(10U, slot.size());
    TestBridge::writeBigEndian32(slot.data(), 0x200U);
    TestBridge::writeBigEndian32(slot.data() + 4U, 2U);
    slot[8U] = 0x11U;
    slot[9U] = 0x22U;
    bridge.txWriter().commit();

    uint32_t seenId    = 0U;
    uint16_t seenLen   = 0U;
    uint8_t seen0      = 0U;
    uint8_t seen1      = 0U;
    bool const accepted = bridge.drainTxFrame(
        [&seenId, &seenLen, &seen0, &seen1](uint32_t id, uint16_t len, uint8_t const* data) {
            seenId  = id;
            seenLen = len;
            seen0   = data[0];
            seen1   = data[1];
            return true;
        });
    EXPECT_TRUE(accepted);
    EXPECT_EQ(0x200U, seenId);
    EXPECT_EQ(2U, seenLen);
    EXPECT_EQ(0x11U, seen0);
    EXPECT_EQ(0x22U, seen1);
    EXPECT_FALSE(bridge.drainTxFrame([](uint32_t, uint16_t, uint8_t const*) { return true; }));
}

/**
 * \desc: Sink rejections count as TX drops.
 */
TEST(IoBridgeTest, drain_reject_counts)
{
    TestBridge bridge;
    ::etl::span<uint8_t> slot = bridge.txWriter().allocate(8U);
    ASSERT_EQ(8U, slot.size());
    TestBridge::writeBigEndian32(slot.data(), 0x200U);
    TestBridge::writeBigEndian32(slot.data() + 4U, 0U);
    bridge.txWriter().commit();

    EXPECT_FALSE(bridge.drainTxFrame([](uint32_t, uint16_t, uint8_t const*) { return false; }));
    EXPECT_EQ(1U, bridge.getTxDropCount());
}

} // namespace
