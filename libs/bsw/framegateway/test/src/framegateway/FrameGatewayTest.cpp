/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/FrameGateway.h"

#include <gmock/gmock.h>

#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

::framegateway::FrameConfig makeFrame(uint32_t id, ::framegateway::GatewayAction action)
{
    ::framegateway::FrameConfig frame{};
    frame.frameId     = id;
    frame.channelType = 0U;
    frame.channelId   = 0U;
    frame.frameLength = 64U;
    frame.action      = action;
    ::framegateway::PduInFrame p1{};
    p1.pduId = 1001U;
    p1.length = 12U;
    ::framegateway::PduInFrame p2{};
    p2.pduId           = 1002U;
    p2.startByteOffset = 12U;
    p2.length          = 20U;
    ::framegateway::PduInFrame p3{};
    p3.pduId           = 1003U;
    p3.startByteOffset = 32U;
    p3.length          = 32U;
    frame.pdus         = {p1, p2, p3};
    return frame;
}

/**
 * \desc: ROUTE_ONLY forwards every PDU to its registered destinations.
 */
TEST(FrameGatewayTest, route_only)
{
    ::framegateway::FrameGateway gateway;
    gateway.init({makeFrame(0x500U, ::framegateway::GatewayAction::ROUTE_ONLY)});

    std::vector<std::tuple<uint8_t, uint8_t, uint32_t, uint16_t>> sent;
    gateway.setTxSender(
        [&sent](uint8_t type, uint8_t ch, uint32_t id, uint16_t len, uint8_t const*) {
            sent.emplace_back(type, ch, id, len);
            return true;
        });
    ::framegateway::GatewayDestination dest{};
    dest.channelType = 2U;
    dest.frameId     = 0x1000U;
    gateway.registerPduRoute(1001U, {dest});
    gateway.registerPduRoute(1002U, {dest});
    gateway.registerPduRoute(1003U, {dest});

    uint8_t frame[64U] = {0U};
    gateway.onFrameReceived(0U, 0U, 0x500U, 64U, frame);
    ASSERT_EQ(3U, sent.size());
    EXPECT_EQ(12U, std::get<3>(sent[0]));
    EXPECT_EQ(20U, std::get<3>(sent[1]));
    EXPECT_EQ(32U, std::get<3>(sent[2]));
    EXPECT_EQ(3U, gateway.getRoutedPduCount());
    EXPECT_EQ(0U, gateway.getExtractedPduCount());
}

/**
 * \desc: BOTH consumes PDUs locally and routes them.
 */
TEST(FrameGatewayTest, both_action)
{
    ::framegateway::FrameGateway gateway;
    gateway.init({makeFrame(0x500U, ::framegateway::GatewayAction::BOTH)});

    std::vector<uint32_t> sunk;
    gateway.setPduSink([&sunk](uint32_t pduId, uint16_t, uint8_t const*) {
        sunk.push_back(pduId);
    });
    uint32_t txCount = 0U;
    gateway.setTxSender([&txCount](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
        txCount++;
        return true;
    });
    ::framegateway::GatewayDestination dest{};
    gateway.registerPduRoute(1001U, {dest});

    uint8_t frame[64U] = {0U};
    gateway.onFrameReceived(0U, 0U, 0x500U, 64U, frame);
    EXPECT_EQ(3U, sunk.size());
    EXPECT_EQ(1U, txCount);
    EXPECT_EQ(3U, gateway.getExtractedPduCount());
}

/**
 * \desc: Unknown frames and oversize lengths are dropped and counted.
 */
TEST(FrameGatewayTest, drops_unknown_frames)
{
    ::framegateway::FrameGateway gateway;
    gateway.init({makeFrame(0x500U, ::framegateway::GatewayAction::ROUTE_ONLY)});

    uint8_t frame[64U] = {0U};
    gateway.onFrameReceived(0U, 0U, 0x501U, 64U, frame);
    gateway.onFrameReceived(0U, 0U, 0x500U, 65U, frame);
    EXPECT_EQ(2U, gateway.getDroppedFrameCount());
}

/**
 * \desc: A DENY policy drops the frame before extraction.
 */
TEST(FrameGatewayTest, policy_deny)
{
    ::framegateway::FrameGateway gateway;
    gateway.init({makeFrame(0x500U, ::framegateway::GatewayAction::BOTH)});
    ::framegateway::PolicyEngine policy;
    ::framegateway::GatewayPolicy rule{};
    rule.frameId = 0x500U;
    rule.action  = ::framegateway::PolicyAction::DENY;
    policy.addPolicy(rule);
    gateway.setPolicyEngine(&policy);

    uint32_t sunk = 0U;
    gateway.setPduSink([&sunk](uint32_t, uint16_t, uint8_t const*) { sunk++; });
    uint8_t frame[64U] = {0U};
    gateway.onFrameReceived(0U, 0U, 0x500U, 64U, frame);
    EXPECT_EQ(0U, sunk);
    EXPECT_EQ(1U, gateway.getDroppedFrameCount());
}

/**
 * \desc: Oversize PDUs go to the transport protocol, small ones route direct.
 */
TEST(FrameGatewayTest, tp_handoff)
{
    ::framegateway::FrameGateway gateway;
    gateway.init({makeFrame(0x500U, ::framegateway::GatewayAction::BOTH)});
    gateway.setMaxSingleFramePayload(16U);

    uint32_t tpCalls = 0U;
    gateway.setTpForwarder([&tpCalls](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
        tpCalls++;
    });
    uint32_t routed = 0U;
    gateway.setTxSender([&routed](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
        routed++;
        return true;
    });
    ::framegateway::GatewayDestination dest{};
    gateway.registerPduRoute(1001U, {dest}); // 12 B: direct
    gateway.registerPduRoute(1002U, {dest}); // 20 B: TP
    gateway.registerPduRoute(1003U, {dest}); // 32 B: TP

    uint8_t frame[64U] = {0U};
    gateway.onFrameReceived(0U, 0U, 0x500U, 64U, frame);
    EXPECT_EQ(1U, routed);
    EXPECT_EQ(2U, tpCalls);
}

/**
 * \desc: Stored PDUs pack into the frame at their offsets and transmit.
 */
TEST(FrameGatewayTest, pack_and_transmit)
{
    ::framegateway::FrameGateway gateway;
    gateway.init({makeFrame(0x500U, ::framegateway::GatewayAction::ROUTE_ONLY)});

    std::vector<uint8_t> txData;
    gateway.setTxSender([&txData](uint8_t, uint8_t, uint32_t, uint16_t len, uint8_t const* data) {
        txData.assign(data, data + len);
        return true;
    });

    uint8_t p1[12U] = {0xAAU};
    uint8_t p3[32U] = {0xBBU};
    EXPECT_TRUE(gateway.storePdu(1001U, 12U, p1));
    EXPECT_TRUE(gateway.storePdu(1003U, 32U, p3));
    EXPECT_FALSE(gateway.storePdu(1002U, 4U, nullptr));
    ASSERT_TRUE(gateway.packAndTransmit(0U, 0U, 0x500U));
    ASSERT_EQ(64U, txData.size());
    EXPECT_EQ(0xAAU, txData[0]);
    EXPECT_EQ(0xBBU, txData[32]);
    EXPECT_EQ(0xFFU, txData[12]); // missing PDU keeps padding
    EXPECT_FALSE(gateway.packAndTransmit(0U, 0U, 0x999U));
}

} // namespace
