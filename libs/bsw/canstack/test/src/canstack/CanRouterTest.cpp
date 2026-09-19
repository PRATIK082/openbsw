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
#include "canstack/CanRouter.h"

#include <gmock/gmock.h>

#include <cmath>
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

/**
 * \desc: Rules without transform and with identical frame ids forward the
 * payload unmodified to all destinations.
 */
TEST(CanRouterTest, frame_level_forwarding)
{
    ::canstack::CanRouter router;
    std::vector<TxRecord> tx;

    router.setTransmitFunction(
        [&tx](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
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

    ::canstack::RoutingRule rule;
    rule.inputChannel  = 0U;
    rule.inputFrameId = 0x100U;
    rule.outputDestinations = {{1U, 0x100U}, {2U, 0x100U}};
    router.addRoutingRule(rule);

    uint8_t const data[3] = {0x01U, 0x02U, 0x03U};
    router.onFrameReceived(0U, 0x100U, 3U, data);

    ASSERT_EQ(2U, tx.size());
    EXPECT_EQ(1U, tx[0].channelId);
    EXPECT_EQ(2U, tx[1].channelId);
    EXPECT_EQ(0x100U, tx[0].frameId);
    EXPECT_THAT(tx[0].data, ElementsAre(0x01U, 0x02U, 0x03U));
}

/**
 * \desc: Frames that do not match a rule are not forwarded.
 */
TEST(CanRouterTest, unmatched_frames_are_dropped)
{
    ::canstack::CanRouter router;
    size_t txCount = 0U;
    router.setTransmitFunction([&txCount](uint8_t, uint32_t, uint8_t, uint8_t const*) {
        ++txCount;
        return true;
    });

    ::canstack::RoutingRule rule;
    rule.inputChannel  = 0U;
    rule.inputFrameId = 0x100U;
    rule.outputDestinations = {{1U, 0x100U}};
    router.addRoutingRule(rule);

    uint8_t const data[1] = {0U};
    router.onFrameReceived(0U, 0x200U, 1U, data);
    router.onFrameReceived(7U, 0x100U, 1U, data);

    EXPECT_EQ(0U, txCount);
}

/**
 * \desc: Signal level gateway: a frame with a different id (and different
 * channel) is rebuilt from the matching signal names.
 */
TEST(CanRouterTest, signal_level_gateway)
{
    ::canstack::SignalDatabase db;
    db.loadFrameTable(::canstack::testutils::DEMO_FRAMES, ::canstack::testutils::DEMO_FRAME_COUNT);
    db.loadSignalTable(::canstack::testutils::DEMO_SIGNALS, ::canstack::testutils::DEMO_SIGNAL_COUNT);

    // Mirror layout on channel 1 with a different frame id.
    ::canstack::FrameTableEntry const mirroredFrames[] = {{0x500U, "EngineDataMirror", 8U, 1U}};
    ::canstack::SignalTableEntry const mirroredSignals[]
        = {{"EngineSpeed", 0x500U, 7U, 16U, true, 0.25, 0.0, 0.0, 8000.0, "rpm", 1U, false, -1,
            false}};
    db.loadFrameTable(mirroredFrames, 1U);
    db.loadSignalTable(mirroredSignals, 1U);

    ::canstack::CanRouter router;
    router.setDatabase(&db);

    std::vector<TxRecord> tx;
    router.setTransmitFunction(
        [&tx](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
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

    ::canstack::RoutingRule rule;
    rule.inputChannel  = 0U;
    rule.inputFrameId = 0x100U;
    rule.outputDestinations = {{1U, 0x500U}};
    router.addRoutingRule(rule);

    // EngineSpeed = 1000.0 rpm -> raw = 4000.
    uint8_t data[8] = {};
    db.packSignal(data, *db.getSignalByName("EngineSpeed"), 1000.0);

    router.onFrameReceived(0U, 0x100U, 8U, data);

    ASSERT_EQ(1U, tx.size());
    EXPECT_EQ(1U, tx[0].channelId);
    EXPECT_EQ(0x500U, tx[0].frameId);

    // The mirrored frame contains the same EngineSpeed value.
    FrameConfig const* mirrorFrame = db.getFrameByChannelAndId(1U, 0x500U);
    ASSERT_NE(nullptr, mirrorFrame);
    double const routed = db.unpackSignal(tx[0].data.data(), mirrorFrame->signals[0]);
    EXPECT_DOUBLE_EQ(1000.0, routed);
}

/**
 * \desc: A transform is applied to every forwarded signal value.
 */
TEST(CanRouterTest, transform_is_applied)
{
    ::canstack::SignalDatabase db;
    db.loadFrameTable(::canstack::testutils::DEMO_FRAMES, ::canstack::testutils::DEMO_FRAME_COUNT);
    db.loadSignalTable(::canstack::testutils::DEMO_SIGNALS, ::canstack::testutils::DEMO_SIGNAL_COUNT);

    // Output frame with the same layout but different id on channel 1.
    ::canstack::FrameTableEntry const outputFrames[] = {{0x501U, "TargetDataOut", 8U, 1U}};
    ::canstack::SignalTableEntry const outputSignals[]
        = {{"TargetSpeed", 0x501U, 0U, 16U, false, 0.1, 0.0, 0.0, 1000.0, "kph", 1U, false, -1,
            false}};
    db.loadFrameTable(outputFrames, 1U);
    db.loadSignalTable(outputSignals, 1U);

    ::canstack::CanRouter router;
    router.setDatabase(&db);

    std::vector<TxRecord> tx;
    router.setTransmitFunction(
        [&tx](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
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

    ::canstack::RoutingRule rule;
    rule.inputChannel  = 0U;
    rule.inputFrameId = 0x200U;
    rule.outputDestinations = {{1U, 0x501U}};
    rule.transform = [](double value) { return value * 2.0; };
    router.addRoutingRule(rule);

    uint8_t data[8] = {};
    FrameConfig const* inputFrame = db.getFrameByChannelAndId(0U, 0x200U);
    ASSERT_NE(nullptr, inputFrame);
    db.packSignal(data, inputFrame->signals[0], 500.0);

    router.onFrameReceived(0U, 0x200U, 8U, data);

    ASSERT_EQ(1U, tx.size());
    FrameConfig const* outputFrame = db.getFrameByChannelAndId(1U, 0x501U);
    ASSERT_NE(nullptr, outputFrame);
    double const routed = db.unpackSignal(tx[0].data.data(), outputFrame->signals[0]);
    EXPECT_DOUBLE_EQ(1000.0, routed);
}

/**
 * \desc: A transform returning NaN drops the signal from the routed frame.
 */
TEST(CanRouterTest, nan_transform_drops_signal)
{
    ::canstack::SignalDatabase db;
    db.loadFrameTable(::canstack::testutils::DEMO_FRAMES, ::canstack::testutils::DEMO_FRAME_COUNT);
    db.loadSignalTable(::canstack::testutils::DEMO_SIGNALS, ::canstack::testutils::DEMO_SIGNAL_COUNT);

    ::canstack::FrameTableEntry const outputFrames[] = {{0x502U, "TargetDataOut", 8U, 1U}};
    ::canstack::SignalTableEntry const outputSignals[]
        = {{"TargetSpeed", 0x502U, 0U, 16U, false, 0.1, 0.0, 0.0, 1000.0, "kph", 1U, false, -1,
            false}};
    db.loadFrameTable(outputFrames, 1U);
    db.loadSignalTable(outputSignals, 1U);

    ::canstack::CanRouter router;
    router.setDatabase(&db);

    std::vector<TxRecord> tx;
    router.setTransmitFunction(
        [&tx](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
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

    ::canstack::RoutingRule rule;
    rule.inputChannel  = 0U;
    rule.inputFrameId = 0x200U;
    rule.outputDestinations = {{1U, 0x502U}};
    rule.transform = [](double) { return std::nan(""); };
    router.addRoutingRule(rule);

    uint8_t data[8] = {};
    FrameConfig const* inputFrame = db.getFrameByChannelAndId(0U, 0x200U);
    ASSERT_NE(nullptr, inputFrame);
    db.packSignal(data, inputFrame->signals[0], 500.0);

    router.onFrameReceived(0U, 0x200U, 8U, data);

    ASSERT_EQ(1U, tx.size());
    FrameConfig const* outputFrame = db.getFrameByChannelAndId(1U, 0x502U);
    ASSERT_NE(nullptr, outputFrame);
    double const routed = db.unpackSignal(tx[0].data.data(), outputFrame->signals[0]);
    EXPECT_DOUBLE_EQ(0.0, routed);
}

/**
 * \desc: clearRules() removes all routing rules.
 */
TEST(CanRouterTest, clear_rules)
{
    ::canstack::CanRouter router;

    ::canstack::RoutingRule rule;
    rule.inputChannel  = 0U;
    rule.inputFrameId = 0x100U;
    router.addRoutingRule(rule);

    EXPECT_EQ(1U, router.getRuleCount());
    router.clearRules();
    EXPECT_EQ(0U, router.getRuleCount());
}

} // namespace
