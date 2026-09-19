/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "canstack/TestUtils.h"
#include "canstack/CanSubsystem.h"

#include <async/AsyncMock.h>
#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

using Transition = ::lifecycle::ILifecycleComponent::Transition;

/**
 * Test double re-exposing the protected lifecycle transitions for direct calls.
 */
class TestSubsystem : public ::canstack::CanSubsystem
{
public:
    using ::canstack::CanSubsystem::init;
    using ::canstack::CanSubsystem::run;
    using ::canstack::CanSubsystem::shutdown;
};

::canstack::CanHwStub& stubDriver()
{
    static ::canstack::CanHwStub hw;
    return hw;
}

::canstack::CanStackConfig makeConfig()
{
    static ::canstack::CanChannelConfig const channels[] = {{0U, &stubDriver(), 500000U, 2U}};

    ::canstack::CanStackConfig config{};
    config.channelConfigs = channels;
    config.channelCount    = 1U;
    config.frameTable      = ::canstack::testutils::DEMO_FRAMES;
    config.frameCount      = ::canstack::testutils::DEMO_FRAME_COUNT;
    config.signalTable     = ::canstack::testutils::DEMO_SIGNALS;
    config.signalCount     = ::canstack::testutils::DEMO_SIGNAL_COUNT;
    return config;
}

/**
 * \desc: The subsystem builds the stack during init and provides the
 * application API afterwards; shutdown tears it down.
 */
TEST(CanSubsystemTest, init_builds_stack)
{
    TestSubsystem subsystem;
    subsystem.configure(makeConfig(), ::async::CONTEXT_INVALID);

    subsystem.init();

    EXPECT_TRUE(subsystem.getCanInterface().isInitialized());
    EXPECT_EQ(1U, subsystem.getCanInterface().getChannelCount());

    subsystem.shutdown();
    EXPECT_FALSE(subsystem.getCanInterface().isInitialized());
}

/**
 * \desc: Without configuration the subsystem completes the init transition
 * without bringing up a stack.
 */
TEST(CanSubsystemTest, unconfigured_init_is_a_no_op)
{
    TestSubsystem subsystem;

    subsystem.init();

    EXPECT_FALSE(subsystem.getCanInterface().isInitialized());
    EXPECT_EQ(0U, subsystem.getCanInterface().getChannelCount());
}

/**
 * \desc: The transition context returned to the LifecycleManager is the one
 * from configure().
 */
TEST(CanSubsystemTest, transition_context_from_configuration)
{
    TestSubsystem subsystem;

    EXPECT_EQ(::async::CONTEXT_INVALID, subsystem.getTransitionContext(Transition::Type::INIT));

    subsystem.configure(makeConfig(), 1U);
    EXPECT_EQ(1U, subsystem.getTransitionContext(Transition::Type::RUN));
}

/**
 * \desc: run() schedules the periodic main functions with the 1 ms period;
 * executing the scheduled runnable drives tx and rx.
 */
TEST(CanSubsystemTest, run_schedules_periodic_main_functions)
{
    ::testing::StrictMock<::async::AsyncMock> asyncMock;
    TestSubsystem subsystem;
    subsystem.configure(makeConfig(), 3U);

    subsystem.init();

    ::canstack::TxSignalEntry entry;
    entry.signalName  = "TargetSpeed";
    entry.frameId     = 0x200U;
    entry.channelId   = 0U;
    entry.cycleTimeMs = 5U;
    subsystem.getCanInterface().getTxScheduler().registerSignal(entry);
    (void)subsystem.getCanInterface().sendSignal("TargetSpeed", 500.0);

    ::async::RunnableType* scheduled = nullptr;
    EXPECT_CALL(
        asyncMock,
        scheduleAtFixedRate(
            3U, _, _, ::canstack::CANSTACK_RUN_PERIOD_MS, ::async::TimeUnit::MILLISECONDS))
        .WillOnce(Invoke([&scheduled](
                            ::async::ContextType,
                            ::async::RunnableType& runnable,
                            ::async::TimeoutType&,
                            uint32_t,
                            ::async::TimeUnitType) { scheduled = &runnable; }));

    subsystem.run();

    ASSERT_NE(nullptr, scheduled);

    // Simulate the 1 ms task: after 5 executions the cyclic frame is due and
    // lands in the stub driver's tx log.
    for (uint32_t tick = 0U; tick < 6U; ++tick)
    {
        scheduled->execute();
    }

    ASSERT_EQ(1U, stubDriver().getTxLog().size());
    EXPECT_EQ(0x200U, stubDriver().getTxLog()[0].getFrameId());

    subsystem.shutdown();
}

/**
 * \desc: The singleton accessor returns the same instance.
 */
TEST(CanSubsystemTest, singleton_instance)
{
    EXPECT_EQ(
        &::canstack::CanSubsystem::getInstance(),
        &::canstack::CanSubsystem::getInstance());
}

} // namespace
