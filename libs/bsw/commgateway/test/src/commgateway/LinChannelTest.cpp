/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/LinChannel.h"

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
        txDlcs.push_back(dlc);
        if ((data != nullptr) && (dlc > 0U))
        {
            txFirstBytes.push_back(data[0]);
        }
        return true;
    }
    void registerRxCallback(uint8_t, RxCallback cb) override { rxCb = cb; }
    void registerTxDoneCallback(TxDoneCallback cb) override { txDoneCb = cb; }
    void mainFunction() override { mainFunctionCalls++; }
    void enterSleepMode() override {}
    void wakeup() override {}

    RxCallback rxCb;
    TxDoneCallback txDoneCb;
    std::vector<uint8_t> txPids;
    std::vector<uint8_t> txDlcs;
    std::vector<uint8_t> txFirstBytes;
    uint32_t mainFunctionCalls = 0U;
};

/**
 * \desc: PID parity bits match the LIN 2.x reference vectors.
 */
TEST(LinChannelTest, pid_vectors)
{
    EXPECT_EQ(0x80U, ::commgateway::LinChannel::computePid(0x00U));
    EXPECT_EQ(0xC1U, ::commgateway::LinChannel::computePid(0x01U));
    EXPECT_EQ(0x3CU, ::commgateway::LinChannel::computePid(0x3CU));
    EXPECT_EQ(0x7DU, ::commgateway::LinChannel::computePid(0x3DU));
    EXPECT_TRUE(::commgateway::LinChannel::isPidValid(0x80U));
    EXPECT_FALSE(::commgateway::LinChannel::isPidValid(0x00U));
}

/**
 * \desc: Classic and enhanced checksums match the LIN 2.x algorithm.
 */
TEST(LinChannelTest, checksum_vectors)
{
    uint8_t const data[4U] = {0x01U, 0x02U, 0x03U, 0x04U};
    EXPECT_EQ(0xF5U, ::commgateway::LinChannel::computeChecksum(0x00U, data, 4U, false));
    uint8_t const enhanced = ::commgateway::LinChannel::computeChecksum(0x20U, data, 4U, true);
    uint8_t const classic  = ::commgateway::LinChannel::computeChecksum(0x20U, data, 4U, false);
    EXPECT_NE(enhanced, classic);
}

/**
 * \desc: Scheduled frames are stored and cyclic ones run on mainFunction().
 */
TEST(LinChannelTest, schedule_and_cyclic_tick)
{
    FakeLinHw hw;
    ::commgateway::LinChannel channel;
    channel.init(&hw, 0U);
    ASSERT_TRUE(channel.isInitialized());

    ::commgateway::LinFrameConfig frame{};
    frame.pid             = ::commgateway::LinChannel::computePid(0x20U);
    frame.dlc             = 4U;
    frame.scheduleTimeMs  = 100U;
    frame.associatedSignal = "EngineSpeed";
    channel.scheduleFrame(frame);
    EXPECT_NE(nullptr, channel.findFrame(frame.pid));

    channel.mainFunction(100U);
    ASSERT_EQ(1U, hw.txPids.size());
    EXPECT_EQ(frame.pid, hw.txPids[0]);
    EXPECT_EQ(1U, hw.mainFunctionCalls);
}

/**
 * \desc: Valid frames reach the upper layer, parity errors are counted.
 */
TEST(LinChannelTest, rx_forward_and_parity_error)
{
    FakeLinHw hw;
    ::commgateway::LinChannel channel;
    channel.init(&hw, 1U);

    std::vector<uint8_t> seen;
    channel.registerUpperLayerCallback(
        [&seen](uint8_t channelId, uint8_t pid, uint8_t*, uint8_t) {
            seen.push_back(channelId);
            seen.push_back(pid);
        });

    uint8_t data[2U] = {0xAAU, 0xBBU};
    channel.onFrameReceived(::commgateway::LinChannel::computePid(0x10U), data, 2U);
    ASSERT_EQ(2U, seen.size());
    EXPECT_EQ(1U, seen[0]);

    channel.onFrameReceived(0x10U, data, 2U); // invalid parity
    EXPECT_EQ(1U, channel.getErrorCount(::commgateway::LinError::PARITY));
}

/**
 * \desc: Sleep blocks transmission, failed tx counts a timeout error.
 */
TEST(LinChannelTest, sleep_and_tx_error)
{
    FakeLinHw hw;
    ::commgateway::LinChannel channel;
    channel.init(&hw, 0U);

    uint8_t data[1U] = {0U};
    channel.enterSleepMode();
    EXPECT_TRUE(channel.isSleeping());
    EXPECT_FALSE(channel.transmitFrame(0x01U, data, 1U));

    channel.wakeup();
    EXPECT_FALSE(channel.isSleeping());
    EXPECT_TRUE(channel.transmitFrame(0x01U, data, 1U));

    channel.onFrameTransmitted(0x01U, false);
    EXPECT_EQ(1U, channel.getErrorCount(::commgateway::LinError::TIMEOUT));
}

} // namespace
