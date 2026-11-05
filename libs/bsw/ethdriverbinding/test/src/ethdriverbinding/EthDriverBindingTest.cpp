/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <commgateway/EthIpdu.h>
#include <ethdriverbinding/EthDriverBinding.h>
#include <ethdriverbinding/EthDriverMock.h>

namespace
{
using ethdriverbinding::EthDriverBinding;
using ethdriverbinding::EthDriverMock;
} // namespace

TEST(EthDriverBindingTest, InMemoryLoopback)
{
    EthDriverBinding binding;
    binding.init();
    EXPECT_FALSE(binding.hasEthDriver());

    std::vector<uint8_t> received;
    binding.registerRxCallback([&received](uint32_t srcIp, uint16_t srcPort, uint8_t const* data, uint16_t length) {
        (void)srcIp;
        (void)srcPort;
        received.assign(data, data + length);
    });

    uint8_t tx[3U] = {1U, 2U, 3U};
    ASSERT_TRUE(binding.send(0xC0A80001U, 1234U, 5678U, tx, 3U));
    EXPECT_EQ(binding.getTxLog().size(), 1U);
    EXPECT_EQ(received.size(), 3U);
    EXPECT_EQ(received[1], 2U);

    binding.clearTxLog();
    EXPECT_EQ(binding.getTxLog().size(), 0U);

    uint8_t inj[2U] = {9U, 8U};
    binding.injectRx(0xC0A80002U, 9999U, inj, 2U);
    EXPECT_EQ(received.size(), 2U);
}

TEST(EthDriverBindingTest, DelegatesToEthDriverWhenBound)
{
    EthDriverBinding binding;
    EthDriverMock mock;
    binding.setEthDriver(&mock);

    ASSERT_TRUE(binding.hasEthDriver());

    EXPECT_CALL(mock, init()).WillOnce(::testing::Return(true));
    binding.init();
    EXPECT_TRUE(binding.isInitialized());

    uint8_t tx[2U] = {0xABU, 0xCDU};
    EXPECT_CALL(mock, send(0xC0A80001U, 1234U, 5678U, ::testing::_, 2U)).WillOnce(::testing::Return(true));
    ASSERT_TRUE(binding.send(0xC0A80001U, 1234U, 5678U, tx, 2U));

    ethdriverbinding::EthDriverBinding::RxCallback cb = [](uint32_t, uint16_t, uint8_t const*, uint16_t) {};
    EXPECT_CALL(mock, registerRxCallback(::testing::_)).Times(1);
    binding.registerRxCallback(cb);

    EXPECT_CALL(mock, mainFunction()).Times(1);
    binding.mainFunction();

    EXPECT_CALL(mock, shutdown()).Times(1);
    binding.shutdown();
}

TEST(EthDriverBindingTest, EthIpduManagerUsesBinding)
{
    commgateway::EthIpduManager mgr;
    EthDriverBinding binding;
    commgateway::EthIpduConfig cfg{};
    cfg.destIp = 0xC0A80001U;
    cfg.destPort = 1234U;
    cfg.srcPort = 5678U;
    binding.init();
    mgr.init(cfg, &binding);
    uint8_t data[2U] = {1U, 2U};
    EXPECT_TRUE(mgr.transmitPdu(1U, 2U, data));
    EXPECT_EQ(binding.getTxLog().size(), 1U);
}