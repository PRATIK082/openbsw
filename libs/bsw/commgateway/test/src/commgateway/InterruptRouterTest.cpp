/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/InterruptRouter.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

/**
 * \desc: Registered handlers fire; unregistered sources are a no-op.
 */
TEST(InterruptRouterTest, register_and_dispatch)
{
    auto& router = ::commgateway::InterruptRouter::getInstance();
    router.clear();

    uint32_t calls = 0U;
    router.registerHandler(::commgateway::InterruptSource::CAN0_RX, [&calls]() { calls++; });
    EXPECT_EQ(1U, router.getHandlerCount());

    router.onInterrupt(::commgateway::InterruptSource::CAN0_RX);
    EXPECT_EQ(1U, calls);
    router.onInterrupt(::commgateway::InterruptSource::LIN0_RX); // none registered
    EXPECT_EQ(1U, calls);

    EXPECT_TRUE(router.removeHandler(::commgateway::InterruptSource::CAN0_RX));
    EXPECT_FALSE(router.removeHandler(::commgateway::InterruptSource::CAN0_RX));
    router.onInterrupt(::commgateway::InterruptSource::CAN0_RX);
    EXPECT_EQ(1U, calls);
}

/**
 * \desc: Static ISR wrappers fan out to the matching source handler.
 */
TEST(InterruptRouterTest, isr_wrappers)
{
    auto& router = ::commgateway::InterruptRouter::getInstance();
    router.clear();

    bool linRx = false;
    bool ethTx = false;
    router.registerHandler(::commgateway::InterruptSource::LIN0_RX, [&linRx]() { linRx = true; });
    router.registerHandler(::commgateway::InterruptSource::ETH0_TX, [&ethTx]() { ethTx = true; });

    ::commgateway::InterruptRouter::onLin0RxIsr();
    ::commgateway::InterruptRouter::onEth0TxIsr();
    EXPECT_TRUE(linRx);
    EXPECT_TRUE(ethTx);
    router.clear();
}

} // namespace
