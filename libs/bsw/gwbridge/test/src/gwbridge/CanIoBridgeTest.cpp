/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "gwbridge/CanIoBridge.h"
#include "gwbridge/EthIoBridge.h"
#include "gwbridge/LinIoBridge.h"

#include "canstack/CanChannel.h"
#include "canstack/CanHwStub.h"
#include "commgateway/EthChannel.h"
#include "commgateway/EthIpdu.h"

#include <gmock/gmock.h>

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
        txSizes.push_back(dlc);
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
    std::vector<uint8_t> txSizes;
    std::vector<uint8_t> txBytes;
};

/**
 * \desc: CAN frames injected at the stub appear framed on the bridge reader.
 */
TEST(CanIoBridgeTest, rx_path)
{
    ::canstack::CanHwStub hw;
    ::canstack::CanChannel channel;
    ::canstack::CanChannelConfig config{};
    config.channelId    = 0U;
    config.hwDriver     = &hw;
    config.baudrate     = 500000U;
    config.maxMailboxes = 1U;
    ASSERT_TRUE(channel.init(config));

    ::gwbridge::CanIoBridge bridge;
    bridge.bind(channel);

    uint8_t const payload[2U] = {0xDEU, 0xADU};
    hw.injectRxFrame(0x100U, 2U, payload);
    channel.mainFunction();

    ::etl::span<uint8_t> message = bridge.rxReader().peek();
    ASSERT_EQ(10U, message.size());
    EXPECT_EQ(0x100U, ::gwbridge::CanQueueBridge::readBigEndian32(message.data()));
    EXPECT_EQ(2U, ::gwbridge::CanQueueBridge::readBigEndian32(message.data() + 4U));
    EXPECT_EQ(0xDEU, message[8U]);
    bridge.rxReader().release();
}

/**
 * \desc: Framed TX messages transmit through the channel (loopback stub).
 */
TEST(CanIoBridgeTest, tx_path)
{
    ::canstack::CanHwStub hw;
    hw.setLoopback(false);
    ::canstack::CanChannel channel;
    ::canstack::CanChannelConfig config{};
    config.channelId    = 1U;
    config.hwDriver     = &hw;
    config.baudrate     = 500000U;
    config.maxMailboxes = 1U;
    ASSERT_TRUE(channel.init(config));

    ::gwbridge::CanIoBridge bridge;
    bridge.bind(channel);

    ::etl::span<uint8_t> slot = bridge.txWriter().allocate(9U);
    ASSERT_EQ(9U, slot.size());
    ::gwbridge::CanQueueBridge::writeBigEndian32(slot.data(), 0x200U);
    ::gwbridge::CanQueueBridge::writeBigEndian32(slot.data() + 4U, 1U);
    slot[8U] = 0x55U;
    bridge.txWriter().commit();

    EXPECT_EQ(1U, bridge.pumpTx(channel));
    ASSERT_EQ(1U, hw.getTxLog().size());
    EXPECT_EQ(0x200U, hw.getTxLog()[0].getFrameId());
    EXPECT_EQ(1U, bridge.getTxSentCount());
}

/**
 * \desc: LIN frames bridge with the PID as message id, both directions.
 */
TEST(LinIoBridgeTest, roundtrip)
{
    FakeLinHw hw;
    ::commgateway::LinChannel channel;
    channel.init(&hw, 0U);

    ::gwbridge::LinIoBridge bridge;
    bridge.bind(channel);

    uint8_t rxData[2U] = {0x11U, 0x22U};
    uint8_t const pid  = ::commgateway::LinChannel::computePid(0x10U);
    ASSERT_TRUE(static_cast<bool>(hw.rxCb));
    hw.rxCb(pid, rxData, 2U);

    ::etl::span<uint8_t> message = bridge.rxReader().peek();
    ASSERT_EQ(10U, message.size());
    EXPECT_EQ(pid, ::gwbridge::LinQueueBridge::readBigEndian32(message.data()));
    bridge.rxReader().release();

    ::etl::span<uint8_t> slot = bridge.txWriter().allocate(9U);
    ASSERT_EQ(9U, slot.size());
    ::gwbridge::LinQueueBridge::writeBigEndian32(slot.data(), pid);
    ::gwbridge::LinQueueBridge::writeBigEndian32(slot.data() + 4U, 1U);
    slot[8U] = 0x77U;
    bridge.txWriter().commit();
    EXPECT_EQ(1U, bridge.pumpTx(channel));
    ASSERT_EQ(1U, hw.txPids.size());
    EXPECT_EQ(pid, hw.txPids[0]);
    EXPECT_EQ(0x77U, hw.txBytes[0]);
}

/**
 * \desc: Ethernet PDUs bridge with the PDU id as message id, both directions.
 */
TEST(EthIoBridgeTest, roundtrip)
{
    ::commgateway::EthLoopbackChannel transport;
    transport.setLoopback(false);
    ::commgateway::EthIpduManager manager;
    ::commgateway::EthIpduConfig config{};
    config.primaryPduId = 100U;
    manager.init(config, &transport);

    ::gwbridge::EthIoBridge bridge;
    bridge.bind(manager);

    uint8_t const rxData[3U] = {1U, 2U, 3U};
    transport.injectRx(0U, 4000U, rxData, 3U);

    ::etl::span<uint8_t> message = bridge.rxReader().peek();
    ASSERT_EQ(11U, message.size());
    EXPECT_EQ(100U, ::gwbridge::EthQueueBridge::readBigEndian32(message.data()));
    bridge.rxReader().release();

    ::etl::span<uint8_t> slot = bridge.txWriter().allocate(9U);
    ASSERT_EQ(9U, slot.size());
    ::gwbridge::EthQueueBridge::writeBigEndian32(slot.data(), 100U);
    ::gwbridge::EthQueueBridge::writeBigEndian32(slot.data() + 4U, 1U);
    slot[8U] = 0x99U;
    bridge.txWriter().commit();
    EXPECT_EQ(1U, bridge.pumpTx(manager));
    ASSERT_EQ(1U, transport.getTxLog().size());
    EXPECT_EQ(0x99U, transport.getTxLog()[0].payload[0]);
}

} // namespace
