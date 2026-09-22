/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/TpGateway.h"

#include <gmock/gmock.h>

#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

/// Loops two gateways back-to-back: A transmits, B receives (and FC returns).
struct TpLoop
{
    ::framegateway::TpGateway a;
    ::framegateway::TpGateway b;
    std::vector<std::tuple<uint8_t, uint8_t, uint32_t, std::vector<uint8_t>>> received;

    TpLoop()
    {
        a.init(4U, 4095U);
        b.init(4U, 4095U);
        a.setTxSender([this](uint8_t t, uint8_t c, uint32_t id, uint16_t len, uint8_t const* d) {
            b.onTransportFrameReceived(t, c, id, len, d, 0U);
            return true;
        });
        b.setTxSender([this](uint8_t t, uint8_t c, uint32_t id, uint16_t len, uint8_t const* d) {
            a.onTransportFrameReceived(t, c, id, len, d, 0U); // flow control back to A
            return true;
        });
        b.setPduCallback(
            [this](uint8_t t, uint8_t c, uint32_t id, uint16_t len, uint8_t const* d) {
                received.emplace_back(t, c, id, std::vector<uint8_t>(d, d + len));
            });
    }
};

/**
 * \desc: Short payloads travel in a single frame and reassemble directly.
 */
TEST(TpGatewayTest, single_frame)
{
    TpLoop loop;
    uint8_t const payload[5U] = {1U, 2U, 3U, 4U, 5U};
    EXPECT_NE(0U, loop.a.segmentAndTransmit(0U, 0U, 0x7E0U, 5U, payload));
    ASSERT_EQ(1U, loop.received.size());
    EXPECT_EQ(0x7E0U, std::get<2>(loop.received[0]));
    EXPECT_THAT(std::get<3>(loop.received[0]), ElementsAre(1U, 2U, 3U, 4U, 5U));
}

/**
 * \desc: A 20-byte message segments into FF + CFs and reassembles via FC.
 */
TEST(TpGatewayTest, multi_frame_roundtrip)
{
    TpLoop loop;
    uint8_t payload[20U] = {0U};
    for (uint8_t i = 0U; i < 20U; ++i)
    {
        payload[i] = i;
    }
    EXPECT_NE(0U, loop.a.segmentAndTransmit(0U, 0U, 0x7E0U, 20U, payload));
    ASSERT_EQ(1U, loop.received.size());
    ASSERT_EQ(20U, std::get<3>(loop.received[0]).size());
    for (uint8_t i = 0U; i < 20U; ++i)
    {
        EXPECT_EQ(i, std::get<3>(loop.received[0])[i]);
    }
    EXPECT_EQ(0U, loop.a.getActiveSessionCount());
    EXPECT_EQ(0U, loop.b.getActiveSessionCount());
}

/**
 * \desc: Oversize payloads and missing senders are rejected with id 0.
 */
TEST(TpGatewayTest, rejects_invalid_tx)
{
    ::framegateway::TpGateway gateway;
    gateway.init(1U, 100U);
    uint8_t payload[200U] = {0U};
    EXPECT_EQ(0U, gateway.segmentAndTransmit(0U, 0U, 0x7E0U, 200U, payload)); // > maxPduSize
    EXPECT_EQ(0U, gateway.segmentAndTransmit(0U, 0U, 0x7E0U, 0U, payload));   // empty
    uint8_t small[4U] = {0U};
    EXPECT_EQ(0U, gateway.segmentAndTransmit(0U, 0U, 0x7E0U, 4U, small)); // no sender
    EXPECT_EQ(3U, gateway.getSegmentationErrorCount());
}

/**
 * \desc: A wrong sequence number aborts reassembly and counts an error.
 */
TEST(TpGatewayTest, bad_sequence_aborts)
{
    ::framegateway::TpGateway gateway;
    gateway.init(4U, 4095U);
    gateway.setTxSender([](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
        return true;
    });
    bool delivered = false;
    gateway.setPduCallback(
        [&delivered](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
            delivered = true;
        });

    uint8_t ff[8U] = {0x10U, 0x14U, 1U, 2U, 3U, 4U, 5U, 6U}; // FF, 20 bytes
    gateway.onTransportFrameReceived(0U, 0U, 0x7E0U, 8U, ff, 0U);
    EXPECT_EQ(1U, gateway.getActiveSessionCount());

    uint8_t badCf[8U] = {0x25U, 0U, 0U, 0U, 0U, 0U, 0U, 0U}; // seq 5, expected 1
    gateway.onTransportFrameReceived(0U, 0U, 0x7E0U, 8U, badCf, 10U);
    EXPECT_FALSE(delivered);
    EXPECT_EQ(0U, gateway.getActiveSessionCount());
    EXPECT_EQ(1U, gateway.getSegmentationErrorCount());
}

/**
 * \desc: Stalled reassembly times out via mainFunction().
 */
TEST(TpGatewayTest, reassembly_timeout)
{
    ::framegateway::TpGateway gateway;
    gateway.init(4U, 4095U);
    gateway.setTxSender([](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
        return true;
    });

    uint8_t ff[8U] = {0x10U, 0x14U, 1U, 2U, 3U, 4U, 5U, 6U};
    gateway.onTransportFrameReceived(0U, 0U, 0x7E0U, 8U, ff, 0U);
    ASSERT_EQ(1U, gateway.getActiveSessionCount());

    gateway.mainFunction(999U);
    EXPECT_EQ(1U, gateway.getActiveSessionCount());
    gateway.mainFunction(1000U);
    EXPECT_EQ(0U, gateway.getActiveSessionCount());
    EXPECT_EQ(1U, gateway.getReassemblyTimeoutCount());
}

} // namespace
