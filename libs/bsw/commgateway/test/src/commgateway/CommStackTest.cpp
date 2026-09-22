/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/CommStack.h"
#include "commgateway/EthChannel.h"

#include <gmock/gmock.h>

#include <vector>

namespace
{
using namespace ::testing;

class FakeLinHw : public ::commgateway::LinHwInterface
{
public:
    bool init(uint8_t, uint32_t) override { return true; }
    void shutdown() override {}
    bool transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc) override
    {
        txPids.push_back(pid);
        if ((data != nullptr) && (dlc > 0U))
        {
            txBytes.push_back(data[0]);
        }
        return true;
    }
    void registerRxCallback(uint8_t, RxCallback cb) override { rxCb = cb; }
    void registerTxDoneCallback(TxDoneCallback) override {}
    void mainFunction() override {}
    void enterSleepMode() override {}
    void wakeup() override {}

    RxCallback rxCb;
    std::vector<uint8_t> txPids;
    std::vector<uint8_t> txBytes;
};

class TestCommStack : public ::commgateway::CommStack
{
public:
    void publicInit() { init(); }
    void publicShutdown() { shutdown(); }
};

/**
 * \desc: LIN rx flows through the gateway to Ethernet (CAN disabled).
 */
TEST(CommStackTest, lin_to_eth_gateway_path)
{
    FakeLinHw linHw;
    ::commgateway::EthLoopbackChannel ethTransport;

    ::commgateway::CommStackConfig config{};
    config.canConfig    = nullptr;
    config.linHw        = &linHw;
    config.linChannelId = 0U;
    config.ethTransport = &ethTransport;
    config.ethConfig.primaryPduId = 4096U;

    auto& stack = ::commgateway::CommStack::getInstance();
    stack.configure(config, ::async::CONTEXT_INVALID);
    ASSERT_TRUE(stack.isConfigured());
    static_cast<TestCommStack&>(stack).publicInit();

    ::commgateway::LinFrameConfig frame{};
    frame.pid              = ::commgateway::LinChannel::computePid(0x20U);
    frame.dlc              = 4U;
    frame.associatedSignal = "EngineSpeed";
    stack.getLinChannel().scheduleFrame(frame);

    std::string error;
    ASSERT_TRUE(stack.loadGatewayRules(
        R"({"signalRouting": [{"signalName": "EngineSpeed",
            "destinations": [{"channelType": 2, "channelId": 0}]}]})",
        error)) << error;
    stack.registerEthSignalMapping(4096U, "EthMirror"); // no rule: rx sink, no re-tx loop

    uint8_t data[4U] = {100U, 0U, 0U, 0U};
    linHw.rxCb(frame.pid, data, 4U);

    EXPECT_EQ(1U, ethTransport.getTxLog().size());
    EXPECT_EQ(2U, stack.getStatistics().rxFrameCount); // LIN rx + ETH loopback rx
    EXPECT_EQ(1U, stack.getStatistics().txFrameCount);

    static_cast<TestCommStack&>(stack).publicShutdown();
}

/**
 * \desc: Gateway dispatch reaches the LIN bus for signal-mapped frames.
 */
TEST(CommStackTest, gateway_dispatch_to_lin)
{
    FakeLinHw linHw;
    ::commgateway::EthLoopbackChannel ethTransport;

    ::commgateway::CommStackConfig config{};
    config.linHw        = &linHw;
    config.ethTransport = &ethTransport;
    config.ethConfig.primaryPduId = 1U;

    auto& stack = ::commgateway::CommStack::getInstance();
    stack.configure(config, ::async::CONTEXT_INVALID);
    static_cast<TestCommStack&>(stack).publicInit();

    ::commgateway::LinFrameConfig frame{};
    frame.pid              = ::commgateway::LinChannel::computePid(0x21U);
    frame.dlc              = 2U;
    frame.associatedSignal = "WindowSwitch_Status";
    stack.getLinChannel().scheduleFrame(frame);

    std::string error;
    ASSERT_TRUE(stack.loadGatewayRules(
        R"({"signalRouting": [{"signalName": "WindowSwitch_Status",
            "destinations": [{"channelType": 1, "channelId": 0}]}]})",
        error)) << error;

    stack.getSignalGateway().onSignalUpdate("WindowSwitch_Status", 3.0, 0U);
    ASSERT_EQ(1U, linHw.txPids.size());
    EXPECT_EQ(frame.pid, linHw.txPids[0]);
    ASSERT_EQ(1U, linHw.txBytes.size());
    EXPECT_EQ(3U, linHw.txBytes[0]);

    static_cast<TestCommStack&>(stack).publicShutdown();
}

} // namespace
