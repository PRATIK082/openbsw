/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/PduAssembler.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

::framegateway::PduInFrame makePdu(uint32_t id, uint16_t offset, uint16_t length)
{
    ::framegateway::PduInFrame pdu{};
    pdu.pduId           = id;
    pdu.startByteOffset = offset;
    pdu.length          = length;
    return pdu;
}

/**
 * \desc: Three PDUs pack at their offsets; padding fills the rest.
 */
TEST(PduAssemblerTest, pack_multi_pdu)
{
    std::vector<::framegateway::PduInFrame> layout
        = {makePdu(1001U, 0U, 12U), makePdu(1002U, 12U, 20U), makePdu(1003U, 32U, 32U)};

    uint8_t p1[12U] = {1U};
    uint8_t p2[20U] = {2U};
    uint8_t p3[32U];
    for (uint8_t i = 0U; i < 32U; ++i)
    {
        p3[i] = 3U;
    }
    ::framegateway::PduDataMap data;
    data[1001U] = {12U, p1};
    data[1002U] = {20U, p2};
    data[1003U] = {32U, p3};

    uint8_t frame[64U] = {0U};
    ASSERT_TRUE(::framegateway::PduAssembler::packMultiPdu(frame, 64U, layout, data));
    EXPECT_EQ(1U, frame[0]);
    EXPECT_EQ(2U, frame[12]);
    EXPECT_EQ(3U, frame[32]);
    EXPECT_EQ(3U, frame[63]);
}

/**
 * \desc: Out-of-range layouts fail packing instead of overrunning.
 */
TEST(PduAssemblerTest, pack_rejects_overflow)
{
    std::vector<::framegateway::PduInFrame> layout = {makePdu(1U, 60U, 8U)};
    uint8_t payload[8U]                            = {0U};
    ::framegateway::PduDataMap data;
    data[1U] = {8U, payload};

    uint8_t frame[64U] = {0U};
    EXPECT_FALSE(::framegateway::PduAssembler::packMultiPdu(frame, 64U, layout, data));
    EXPECT_FALSE(::framegateway::PduAssembler::packMultiPdu(nullptr, 64U, layout, data));

    uint8_t shortPayload[4U] = {0U};
    ::framegateway::PduDataMap shortData;
    shortData[1U] = {4U, shortPayload};
    std::vector<::framegateway::PduInFrame> okLayout = {makePdu(1U, 0U, 8U)};
    EXPECT_FALSE(::framegateway::PduAssembler::packMultiPdu(frame, 64U, okLayout, shortData));
}

/**
 * \desc: Unpacked PDUs alias the frame buffer at the right offsets.
 */
TEST(PduAssemblerTest, unpack_multi_pdu)
{
    std::vector<::framegateway::PduInFrame> layout
        = {makePdu(1001U, 0U, 12U), makePdu(1002U, 12U, 20U), makePdu(999U, 60U, 8U)};

    uint8_t frame[64U] = {0U};
    frame[0]           = 0xAAU;
    frame[12]          = 0xBBU;
    ::framegateway::PduDataMap pdus
        = ::framegateway::PduAssembler::unpackMultiPdu(frame, 64U, layout);
    ASSERT_EQ(2U, pdus.size());
    EXPECT_EQ(12U, pdus[1001U].first);
    EXPECT_EQ(0xAAU, pdus[1001U].second[0]);
    EXPECT_EQ(20U, pdus[1002U].first);
    EXPECT_EQ(0xBBU, pdus[1002U].second[0]);
}

/**
 * \desc: Variable-length PDUs read the length byte, clamped to the maximum.
 */
TEST(PduAssemblerTest, variable_length)
{
    uint8_t pdu[8U] = {0x99U, 0x05U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    EXPECT_EQ(5U, ::framegateway::PduAssembler::extractVariableLengthPdu(pdu, 8U, 1U, 8U));
    EXPECT_EQ(4U, ::framegateway::PduAssembler::extractVariableLengthPdu(pdu, 8U, 1U, 4U));
    EXPECT_EQ(0U, ::framegateway::PduAssembler::extractVariableLengthPdu(pdu, 1U, 1U, 8U));
    EXPECT_EQ(0U, ::framegateway::PduAssembler::extractVariableLengthPdu(nullptr, 8U, 0U, 8U));
}

/**
 * \desc: Padding fills only the unused tail with the pad value.
 */
TEST(PduAssemblerTest, padding)
{
    uint8_t frame[8U] = {1U, 2U, 3U, 4U, 0U, 0U, 0U, 0U};
    ::framegateway::PduAssembler::applyPadding(frame, 4U, 8U, 0xFFU);
    EXPECT_THAT(frame,
                ElementsAre(1U, 2U, 3U, 4U, 0xFFU, 0xFFU, 0xFFU, 0xFFU));
    ::framegateway::PduAssembler::applyPadding(frame, 8U, 8U, 0x00U);
    EXPECT_EQ(0xFFU, frame[7]);
}

} // namespace
