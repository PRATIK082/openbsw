/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "canstack/CanHwStub.h"

#include <gmock/gmock.h>

#include <vector>

namespace
{
using namespace ::testing;

/**
 * \desc: The stub accepts every init and reports it.
 */
TEST(CanHwStubTest, init_and_shutdown)
{
    ::canstack::CanHwStub hw;

    EXPECT_FALSE(hw.isInitialized());
    EXPECT_TRUE(hw.init(0U, 500000U));
    EXPECT_TRUE(hw.isInitialized());
    EXPECT_EQ(500000U, hw.getBaudrate());
    EXPECT_EQ(0U, hw.getChannelId());

    hw.shutdown();
    EXPECT_FALSE(hw.isInitialized());
}

/**
 * \desc: transmit() before init is rejected, afterwards frames are logged.
 */
TEST(CanHwStubTest, transmit_is_logged)
{
    ::canstack::CanHwStub hw;
    uint8_t const data[2] = {0x11U, 0x22U};

    EXPECT_FALSE(hw.transmit(0U, 0x100U, 2U, data));

    (void)hw.init(0U, 500000U);
    EXPECT_TRUE(hw.transmit(0U, 0x100U, 2U, data));

    ASSERT_EQ(1U, hw.getTxLog().size());
    EXPECT_EQ(0x100U, hw.getTxLog()[0].getFrameId());
    EXPECT_EQ(2U, hw.getTxLog()[0].getDlc());

    hw.clearTxLog();
    EXPECT_TRUE(hw.getTxLog().empty());
}

/**
 * \desc: Without loopback, transmitted frames are not dispatched to rx callbacks.
 */
TEST(CanHwStubTest, no_loopback_by_default)
{
    ::canstack::CanHwStub hw;
    (void)hw.init(1U, 250000U);

    std::vector<uint32_t> received;
    hw.registerRxCallback(0U, [&received](uint32_t frameId, uint8_t, uint8_t const*) {
        received.push_back(frameId);
    });

    uint8_t const data[1] = {0U};
    (void)hw.transmit(0U, 0x200U, 1U, data);
    hw.mainFunction();

    EXPECT_TRUE(received.empty());
}

/**
 * \desc: With loopback enabled, transmitted frames are dispatched from mainFunction().
 */
TEST(CanHwStubTest, loopback_dispatches_from_main_function)
{
    ::canstack::CanHwStub hw;
    hw.setLoopback(true);
    (void)hw.init(0U, 500000U);

    std::vector<uint32_t> received;
    hw.registerRxCallback(0U, [&received](uint32_t frameId, uint8_t dlc, uint8_t const* data) {
        EXPECT_EQ(2U, dlc);
        EXPECT_EQ(0xABU, data[0]);
        received.push_back(frameId);
    });

    uint8_t const data[2] = {0xABU, 0xCDU};
    (void)hw.transmit(0U, 0x300U, 2U, data);

    EXPECT_TRUE(received.empty());

    hw.mainFunction();
    EXPECT_THAT(received, ElementsAre(0x300U));
}

/**
 * \desc: injectRxFrame() feeds the receive path for non-loopback scenarios.
 * Like a real controller, each frame is delivered once, to the callback of the
 * lowest registered mailbox.
 */
TEST(CanHwStubTest, injected_frames_deliver_to_lowest_mailbox)
{
    ::canstack::CanHwStub hw;
    (void)hw.init(0U, 500000U);

    uint32_t receivedM0 = 0U;
    uint32_t receivedM1 = 0U;
    hw.registerRxCallback(0U, [&receivedM0](uint32_t frameId, uint8_t, uint8_t const*) {
        receivedM0 = frameId;
    });
    hw.registerRxCallback(1U, [&receivedM1](uint32_t frameId, uint8_t, uint8_t const*) {
        receivedM1 = frameId;
    });

    uint8_t const data[1] = {0x55U};
    hw.injectRxFrame(0x111U, 1U, data);
    hw.mainFunction();

    EXPECT_EQ(0x111U, receivedM0);
    EXPECT_EQ(0U, receivedM1);
}

/**
 * \desc: With no callback on mailbox 0, the next registered mailbox receives.
 */
TEST(CanHwStubTest, mailbox_zero_registration_wins)
{
    ::canstack::CanHwStub hw;
    (void)hw.init(0U, 500000U);

    uint32_t receivedM1 = 0U;
    hw.registerRxCallback(1U, [&receivedM1](uint32_t frameId, uint8_t, uint8_t const*) {
        receivedM1 = frameId;
    });

    uint8_t const data[1] = {0x55U};
    hw.injectRxFrame(0x222U, 1U, data);
    hw.mainFunction();

    EXPECT_EQ(0x222U, receivedM1);
}

} // namespace
