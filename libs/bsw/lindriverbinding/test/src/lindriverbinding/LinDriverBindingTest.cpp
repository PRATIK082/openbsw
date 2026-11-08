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

#include <commgateway/LinChannel.h>
#include <lindriverbinding/LinDriverBinding.h>
#include <lindriverbinding/LinDriverMock.h>

namespace
{
using lindriverbinding::LinDriverBinding;
using lindriverbinding::LinDriverMock;
} // namespace

TEST(LinDriverBindingTest, InMemoryTransmitAndInjectRx)
{
    LinDriverBinding binding;
    ASSERT_TRUE(binding.init(1U, 19200U));
    EXPECT_FALSE(binding.hasLinDriver());

    uint8_t data[2U] = {0x11U, 0x22U};
    std::vector<uint8_t> received;
    binding.registerRxCallback(0x3CU, [&received](uint8_t pid, uint8_t* d, uint8_t dlc) {
        (void)pid;
        received.assign(d, d + dlc);
    });
    EXPECT_TRUE(binding.transmitFrame(0x3CU, data, 2U));
    EXPECT_EQ(binding.getTxLog().size(), 1U);
    EXPECT_EQ(received.size(), 2U);
    EXPECT_EQ(received[0], 0x11U);

    binding.injectRx(0x3CU, data, 2U);
    EXPECT_EQ(received.size(), 2U);
}

TEST(LinDriverBindingTest, DelegatesToLinDriverWhenBound)
{
    LinDriverBinding binding;
    LinDriverMock mock;
    binding.setLinDriver(&mock);

    ASSERT_TRUE(binding.hasLinDriver());

    EXPECT_CALL(mock, init(1U, 9600U)).WillOnce(::testing::Return(true));
    ASSERT_TRUE(binding.init(1U, 9600U));

    uint8_t tx[3U] = {1U, 2U, 3U};
    EXPECT_CALL(mock, transmitFrame(0x3DU, ::testing::_, 3U)).WillOnce(::testing::Return(true));
    ASSERT_TRUE(binding.transmitFrame(0x3DU, tx, 3U));

    EXPECT_CALL(mock, enterSleepMode()).Times(1);
    binding.enterSleepMode();
    EXPECT_TRUE(binding.isSleeping());

    EXPECT_CALL(mock, wakeup()).Times(1);
    binding.wakeup();
    EXPECT_FALSE(binding.isSleeping());

    EXPECT_CALL(mock, shutdown()).Times(1);
    binding.shutdown();
}

TEST(LinDriverBindingTest, BindsLinChannel)
{
    commgateway::LinChannel channel;
    LinDriverBinding binding;
    binding.init(2U, 19200U);
    channel.init(&binding, 2U);
    EXPECT_TRUE(channel.isInitialized());

    uint8_t tx[1U] = {0xAAU};
    EXPECT_TRUE(channel.transmitFrame(0x3CU, tx, 1U));
}