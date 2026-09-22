/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/TimeoutMonitor.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

/**
 * \desc: A quiet signal fires its timeout once until activity re-arms it.
 */
TEST(TimeoutMonitorTest, signal_timeout_and_rearm)
{
    ::commgateway::TimeoutMonitor monitor;
    monitor.init();

    uint32_t calls = 0U;
    monitor.registerSignalTimeout("Speed", 100U, [&calls]() { calls++; });
    EXPECT_EQ(1U, monitor.getEntryCount());

    monitor.mainFunction(50U);
    EXPECT_EQ(0U, calls);

    monitor.mainFunction(100U);
    EXPECT_EQ(1U, calls);
    EXPECT_EQ(1U, monitor.getTimeoutCount());

    monitor.mainFunction(200U);
    EXPECT_EQ(1U, calls); // fires only once while quiet

    monitor.notifyRxActivity("Speed", 200U);
    monitor.mainFunction(250U);
    EXPECT_EQ(1U, calls);
    monitor.mainFunction(300U);
    EXPECT_EQ(2U, calls);
}

/**
 * \desc: Frame timeouts track (channel, frame) pairs independently.
 */
TEST(TimeoutMonitorTest, frame_timeout)
{
    ::commgateway::TimeoutMonitor monitor;
    monitor.init();

    bool timedOut = false;
    monitor.registerFrameTimeout(0U, 0x100U, 200U, [&timedOut]() { timedOut = true; });

    monitor.notifyFrameRx(0U, 0x100U, 0U);
    monitor.mainFunction(199U);
    EXPECT_FALSE(timedOut);
    monitor.mainFunction(200U);
    EXPECT_TRUE(timedOut);

    monitor.notifyFrameRx(0U, 0x200U, 500U); // unknown frame: no entry, no crash
    EXPECT_EQ(1U, monitor.getTimeoutCount());
}

} // namespace
