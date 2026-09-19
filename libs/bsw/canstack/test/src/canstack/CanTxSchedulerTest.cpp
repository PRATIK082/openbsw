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
#include "canstack/CanTxScheduler.h"

#include <gmock/gmock.h>

#include <vector>

namespace
{
using namespace ::testing;

using ::canstack::FrameConfig;
using ::canstack::SignalConfig;

struct TxRecord
{
    uint8_t channelId;
    uint32_t frameId;
    uint8_t dlc;
    std::vector<uint8_t> data;
};

class SchedulerEnv
{
public:
    SchedulerEnv()
    {
        db.loadFrameTable(::canstack::testutils::DEMO_FRAMES, ::canstack::testutils::DEMO_FRAME_COUNT);
        db.loadSignalTable(
            ::canstack::testutils::DEMO_SIGNALS, ::canstack::testutils::DEMO_SIGNAL_COUNT);

        scheduler.setDatabase(&db);
        scheduler.setTransmitFunction(
            [this](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
                TxRecord record;
                record.channelId = channelId;
                record.frameId   = frameId;
                record.dlc       = dlc;
                for (uint8_t i = 0U; i < dlc; ++i)
                {
                    record.data.push_back(data[i]);
                }
                tx.push_back(record);
                return true;
            });
    }

    ::canstack::SignalDatabase db;
    ::canstack::CanTxScheduler scheduler;
    std::vector<TxRecord> tx;
};

::canstack::TxSignalEntry makeEntry(
    std::string const& name, uint32_t frameId, uint32_t cycleTimeMs)
{
    ::canstack::TxSignalEntry entry;
    entry.signalName  = name;
    entry.frameId     = frameId;
    entry.channelId   = 0U;
    entry.cycleTimeMs = cycleTimeMs;
    return entry;
}

/**
 * \desc: A registered frame is transmitted every cycleTimeMs, not on every tick.
 */
TEST(CanTxSchedulerTest, cyclic_transmission)
{
    SchedulerEnv env;

    env.scheduler.registerSignal(makeEntry("TargetSpeed", 0x200U, 10U));

    for (uint32_t tick = 0U; tick < 25U; ++tick)
    {
        env.scheduler.mainFunction(); // 1 ms period
    }

    // Registration happens at t=0; transmissions expected at 10, 20 ms.
    EXPECT_EQ(2U, env.tx.size());
    EXPECT_EQ(0x200U, env.tx[0].frameId);
    EXPECT_EQ(8U, env.tx[0].dlc); // from the frame table
}

/**
 * \desc: All signals of a due frame are packed with their current values.
 */
TEST(CanTxSchedulerTest, frame_is_packed_completely)
{
    SchedulerEnv env;

    // Two signals of frame 0x100 with different cycle times: the frame goes
    // out whenever either is due.
    ::canstack::TxSignalEntry engine = makeEntry("EngineSpeed", 0x100U, 10U);
    engine.valueProvider             = []() { return 1000.0; };
    env.scheduler.registerSignal(engine);

    ::canstack::TxSignalEntry vehicle = makeEntry("VehicleSpeed", 0x100U, 30U);
    vehicle.pendingValue              = 123.0;
    vehicle.hasPendingValue           = true;
    env.scheduler.registerSignal(vehicle);

    for (uint32_t tick = 0U; tick < 31U; ++tick)
    {
        env.scheduler.mainFunction();
    }

    ASSERT_GE(env.tx.size(), 1U);

    // First transmission (t=10): EngineSpeed=1000 raw 4000; VehicleSpeed=123 raw 246.
    TxRecord const& first = env.tx.front();
    FrameConfig const* frame = env.db.getFrameByChannelAndId(0U, 0x100U);
    ASSERT_NE(nullptr, frame);

    double const engineValue = env.db.unpackSignal(first.data.data(), frame->signals[0]);
    double const vehicleValue = env.db.unpackSignal(first.data.data(), frame->signals[1]);
    EXPECT_DOUBLE_EQ(1000.0, engineValue);
    EXPECT_DOUBLE_EQ(123.0, vehicleValue);
}

/**
 * \desc: setSignalValue() updates the pending value used when no provider exists.
 */
TEST(CanTxSchedulerTest, set_signal_value_updates_pending)
{
    SchedulerEnv env;

    env.scheduler.registerSignal(makeEntry("TargetSpeed", 0x200U, 5U));
    env.scheduler.setSignalValue("TargetSpeed", 700.0);

    for (uint32_t tick = 0U; tick < 6U; ++tick)
    {
        env.scheduler.mainFunction();
    }

    ASSERT_EQ(1U, env.tx.size());
    FrameConfig const* frame = env.db.getFrameByChannelAndId(0U, 0x200U);
    ASSERT_NE(nullptr, frame);
    double const value = env.db.unpackSignal(env.tx[0].data.data(), frame->signals[0]);
    EXPECT_DOUBLE_EQ(700.0, value);
}

/**
 * \desc: Entries with a zero cycle time are rejected; unknown names in
 * setSignalValue are reported but do not crash.
 */
TEST(CanTxSchedulerTest, invalid_entries_are_rejected)
{
    SchedulerEnv env;

    env.scheduler.registerSignal(makeEntry("EngineSpeed", 0x100U, 0U));
    EXPECT_TRUE(env.scheduler.getEntries().empty());

    env.scheduler.setSignalValue("NotRegistered", 1.0); // must not crash
    EXPECT_EQ(0U, env.tx.size());
}

/**
 * \desc: The time base can be provided externally (e.g. from an OSAL tick).
 */
TEST(CanTxSchedulerTest, external_time_provider)
{
    SchedulerEnv env;

    env.scheduler.registerSignal(makeEntry("TargetSpeed", 0x200U, 100U));

    uint32_t simulatedTime = 0U;
    env.scheduler.setTimeProvider([&simulatedTime]() { return simulatedTime; });

    for (uint32_t tick = 0U; tick < 4U; ++tick)
    {
        simulatedTime += 50U;
        env.scheduler.mainFunction();
    }

    // Due at t=100 (provider based), then again at t=200.
    EXPECT_EQ(2U, env.tx.size());
}

/**
 * \desc: Frames not present in the database are skipped without transmitting.
 */
TEST(CanTxSchedulerTest, unknown_frame_is_skipped)
{
    SchedulerEnv env;

    env.scheduler.registerSignal(makeEntry("GhostSignal", 0x999U, 1U));

    for (uint32_t tick = 0U; tick < 10U; ++tick)
    {
        env.scheduler.mainFunction();
    }

    EXPECT_EQ(0U, env.tx.size());
}

} // namespace
