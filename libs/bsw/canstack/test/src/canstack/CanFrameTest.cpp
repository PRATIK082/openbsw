/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "canstack/CanFrame.h"

#include <gmock/gmock.h>

#include <vector>

namespace
{
using namespace ::testing;

/// Copies the frame payload into a container gmock can match on.
std::vector<uint8_t> payloadOf(::canstack::CanFrame const& frame)
{
    return std::vector<uint8_t>(frame.getData(), frame.getData() + ::canstack::CanFrame::MAX_DATA_LENGTH);
}

/**
 * \desc: A default constructed frame is empty and has a base id.
 */
TEST(CanFrameTest, default_construction)
{
    ::canstack::CanFrame frame;

    EXPECT_EQ(0U, frame.getFrameId());
    EXPECT_EQ(0U, frame.getDlc());
    EXPECT_FALSE(frame.isExtendedId());
}

/**
 * \desc: The id/data constructor copies the payload and keeps the dlc.
 */
TEST(CanFrameTest, construction_with_data)
{
    uint8_t const data[3] = {0xAAU, 0xBBU, 0xCCU};
    ::canstack::CanFrame frame(0x123U, 3U, data);

    EXPECT_EQ(0x123U, frame.getFrameId());
    EXPECT_EQ(3U, frame.getDlc());
    EXPECT_THAT(payloadOf(frame), ElementsAre(0xAAU, 0xBBU, 0xCCU, 0U, 0U, 0U, 0U, 0U));
    EXPECT_FALSE(frame.isExtendedId());
}

/**
 * \desc: Identifiers above 11 bits report the extended format.
 */
TEST(CanFrameTest, extended_id_detection)
{
    ::canstack::CanFrame frame(0x1FFFFFFFU, 0U, nullptr);

    EXPECT_TRUE(frame.isExtendedId());
}

/**
 * \desc: setData replaces dlc and payload, oversized dlc values are rejected.
 */
TEST(CanFrameTest, set_data_validates_dlc)
{
    ::canstack::CanFrame frame;
    uint8_t const data[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};

    frame.setData(8U, data);
    EXPECT_EQ(8U, frame.getDlc());
    EXPECT_THAT(payloadOf(frame), ElementsAre(1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U));

    frame.setData(9U, data);
    EXPECT_EQ(8U, frame.getDlc());
}

/**
 * \desc: Frames with the same id, dlc and payload compare equal.
 */
TEST(CanFrameTest, equality)
{
    uint8_t const data[2] = {0x01U, 0x02U};
    ::canstack::CanFrame frameA(0x42U, 2U, data);
    ::canstack::CanFrame frameB(0x42U, 2U, data);
    ::canstack::CanFrame frameC(0x42U, 1U, data);

    EXPECT_TRUE(frameA == frameB);
    EXPECT_FALSE(frameA != frameB);
    EXPECT_FALSE(frameA == frameC);
}

} // namespace
