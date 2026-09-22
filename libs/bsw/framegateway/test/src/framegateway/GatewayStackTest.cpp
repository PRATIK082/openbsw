/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/GatewayStack.h"

#include <gmock/gmock.h>

#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

class TestGatewayStack : public ::framegateway::GatewayStack
{
public:
    void publicInit() { init(); }
    void publicShutdown() { shutdown(); }
};

::framegateway::GatewayStackConfig makeConfig()
{
    ::framegateway::GatewayStackConfig config{};
    ::framegateway::FrameConfig frame{};
    frame.frameId     = 0x500U;
    frame.frameLength = 64U;
    frame.action      = ::framegateway::GatewayAction::BOTH;
    ::framegateway::PduInFrame pdu{};
    pdu.pduId  = 1001U;
    pdu.length = 12U;
    frame.pdus = {pdu};
    config.frameTable.push_back(frame);
    return config;
}

/**
 * \desc: Frames flow through policy, extraction and PDU routing with stats.
 */
TEST(GatewayStackTest, frame_to_pdu_path)
{
    auto& stack = ::framegateway::GatewayStack::getInstance();
    stack.configure(makeConfig(), ::async::CONTEXT_INVALID);
    ASSERT_TRUE(stack.isConfigured());
    static_cast<TestGatewayStack&>(stack).publicInit();
    stack.resetStatistics();

    std::vector<std::tuple<uint8_t, uint8_t, uint32_t, uint16_t>> sent;
    stack.setTxSender(
        [&sent](uint8_t t, uint8_t c, uint32_t id, uint16_t len, uint8_t const*) {
            sent.emplace_back(t, c, id, len);
            return true;
        });
    uint32_t sunk = 0U;
    stack.getFrameGateway().setPduSink([&sunk](uint32_t, uint16_t, uint8_t const*) { sunk++; });
    ::framegateway::GatewayDestination dest{};
    dest.channelType = 2U;
    dest.frameId     = 0x1000U;
    stack.getFrameGateway().registerPduRoute(1001U, {dest});

    uint8_t frame[64U] = {0U};
    stack.onFrameReceived(0U, 0U, 0x500U, 64U, frame);
    EXPECT_EQ(1U, sent.size());
    EXPECT_EQ(1U, sunk);
    ::framegateway::GatewayStatistics const stats = stack.getStatistics();
    EXPECT_EQ(1U, stats.framesReceived);
    EXPECT_EQ(1U, stats.pdusRouted);
    EXPECT_EQ(1U, stats.pdusExtracted);

    // Unknown frame: dropped.
    stack.onFrameReceived(0U, 0U, 0x501U, 8U, frame);
    EXPECT_EQ(1U, stack.getStatistics().framesDropped);

    static_cast<TestGatewayStack&>(stack).publicShutdown();
}

/**
 * \desc: JSON config loads frame table, diag sessions and policies.
 */
TEST(GatewayStackTest, load_config)
{
    auto& stack = ::framegateway::GatewayStack::getInstance();
    stack.configure(::framegateway::GatewayStackConfig(), ::async::CONTEXT_INVALID);
    static_cast<TestGatewayStack&>(stack).publicInit();
    stack.resetStatistics();

    std::string error;
    ASSERT_TRUE(stack.loadConfig(
        R"({
          "frameConfigs": [
            {"frameId": 1280, "channelType": 0, "channelId": 0,
             "frameLength": 64, "action": "ROUTE_ONLY",
             "pdus": [{"pduId": 1001, "startByteOffset": 0, "length": 12}]}
          ],
          "diagnosticSessions": [
            {"channelType": 0, "channelId": 0, "requestFrameId": 2016,
             "responseFrameId": 2024}
          ],
          "gatewayPolicies": [
            {"frameId": 2015, "channelType": 0, "channelId": 0, "action": "DENY",
             "filter": "source != 0xF1"}
          ]
        })",
        error)) << error;
    EXPECT_NE(nullptr, stack.getFrameGateway().findFrame(0U, 0U, 1280U));
    EXPECT_EQ(1U, stack.getDiagLink().getSessionCount());
    EXPECT_EQ(1U, stack.getPolicyEngine().getPolicyCount());

    // Denied diagnostic frame never reaches the bus.
    uint8_t frame[8U] = {0x01U};
    stack.onFrameReceived(0U, 0U, 2015U, 8U, frame);
    EXPECT_EQ(1U, stack.getStatistics().framesDropped);

    static_cast<TestGatewayStack&>(stack).publicShutdown();
}

/**
 * \desc: TP frames reassemble through the stack transport entry point.
 */
TEST(GatewayStackTest, tp_path)
{
    auto& stack = ::framegateway::GatewayStack::getInstance();
    stack.configure(::framegateway::GatewayStackConfig(), ::async::CONTEXT_INVALID);
    static_cast<TestGatewayStack&>(stack).publicInit();
    stack.resetStatistics();

    bool delivered = false;
    stack.getTpGateway().setPduCallback(
        [&delivered](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
            delivered = true;
        });

    uint8_t sf[8U] = {0x03U, 0x22U, 0xF1U, 0x90U, 0U, 0U, 0U, 0U};
    stack.onTransportFrameReceived(0U, 0U, 0x7E0U, 8U, sf);
    EXPECT_TRUE(delivered);
    EXPECT_EQ(1U, stack.getStatistics().framesReceived);

    static_cast<TestGatewayStack&>(stack).publicShutdown();
}

} // namespace
