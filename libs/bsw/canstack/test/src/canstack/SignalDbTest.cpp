/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "canstack/SignalDb.h"

#include "canstack/TestUtils.h"

#include <gmock/gmock.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace
{
using namespace ::testing;

using ::canstack::FrameConfig;
using ::canstack::SignalConfig;

SignalConfig makeSignal(
    uint8_t startBit, uint8_t length, bool isMotorola, double factor = 1.0, double offset = 0.0,
    bool isSigned = false)
{
    SignalConfig signal;
    signal.signalName = "TestSignal";
    signal.frameId    = 0x100U;
    signal.startBit   = startBit;
    signal.length     = length;
    signal.isMotorola = isMotorola;
    signal.factor     = factor;
    signal.offset     = offset;
    signal.min        = -1.0e12;
    signal.max        = 1.0e12;
    signal.channelId = 0U;
    signal.isSigned   = isSigned;
    return signal;
}

/**
 * \desc: Intel (little endian) signals are packed LSB first from the start bit.
 */
TEST(SignalDbTest, pack_unpack_intel)
{
    ::canstack::SignalDatabase db;
    SignalConfig const signal = makeSignal(0U, 16U, false);

    uint8_t data[8] = {};
    db.packSignal(data, signal, 0x1234);

    EXPECT_EQ(0x34U, data[0]);
    EXPECT_EQ(0x12U, data[1]);
    EXPECT_EQ(0x1234, db.unpackSignal(data, signal));
}

/**
 * \desc: Motorola signals with start bit 7 span the bytes in big endian order.
 */
TEST(SignalDbTest, pack_unpack_motorola)
{
    ::canstack::SignalDatabase db;
    SignalConfig const signal = makeSignal(7U, 16U, true);

    uint8_t data[8] = {};
    db.packSignal(data, signal, 0xABCD);

    EXPECT_EQ(0xABU, data[0]);
    EXPECT_EQ(0xCDU, data[1]);
    EXPECT_EQ(0xABCD, db.unpackSignal(data, signal));
}

/**
 * \desc: Intel signals starting mid-byte pack across byte boundaries.
 */
TEST(SignalDbTest, pack_unpack_intel_offset_start)
{
    ::canstack::SignalDatabase db;
    SignalConfig const signal = makeSignal(4U, 8U, false);

    uint8_t data[8] = {};
    db.packSignal(data, signal, 0xFF);

    // bits 4..11: low nibble of data[0], high nibble of data[1]
    EXPECT_EQ(0xF0U, data[0]);
    EXPECT_EQ(0x0FU, data[1]);
    EXPECT_EQ(0xFF, db.unpackSignal(data, signal));
}

/**
 * \desc: Factor and offset are applied in both directions.
 */
TEST(SignalDbTest, factor_and_offset)
{
    ::canstack::SignalDatabase db;
    SignalConfig const signal = makeSignal(0U, 16U, false, 0.25, 100.0);

    uint8_t data[8] = {};
    db.packSignal(data, signal, 200.0); // raw = (200 - 100) / 0.25 = 400

    EXPECT_EQ(400.0, static_cast<double>(data[0]) + 256.0 * data[1]);
    EXPECT_DOUBLE_EQ(200.0, db.unpackSignal(data, signal));
}

/**
 * \desc: Signed signals are sign extended on unpack and two's complement
 * encoded on pack.
 */
TEST(SignalDbTest, signed_signal)
{
    ::canstack::SignalDatabase db;
    SignalConfig const signal = makeSignal(0U, 8U, false, 1.0, 0.0, true);

    uint8_t data[8] = {};
    db.packSignal(data, signal, -1.0);
    EXPECT_EQ(0xFFU, data[0]);

    EXPECT_DOUBLE_EQ(-1.0, db.unpackSignal(data, signal));
}

/**
 * \desc: Values outside [min, max] are clamped before packing.
 */
TEST(SignalDbTest, clamping)
{
    ::canstack::SignalDatabase db;
    SignalConfig signal = makeSignal(0U, 8U, false);
    signal.min = 0.0;
    signal.max = 100.0;

    uint8_t data[8] = {};
    db.packSignal(data, signal, 300.0);
    EXPECT_EQ(100U, data[0]);

    db.packSignal(data, signal, -5.0);
    EXPECT_EQ(0U, data[0]);
}

/**
 * \desc: Code-first tables are loaded and grouped into frames.
 */
TEST(SignalDbTest, load_code_first_tables)
{
    ::canstack::SignalDatabase db;
    db.loadFrameTable(::canstack::testutils::DEMO_FRAMES, ::canstack::testutils::DEMO_FRAME_COUNT);
    db.loadSignalTable(::canstack::testutils::DEMO_SIGNALS, ::canstack::testutils::DEMO_SIGNAL_COUNT);

    EXPECT_EQ(3U, db.getFrameCount());
    EXPECT_EQ(4U, db.getSignalCount());

    FrameConfig const* frame = db.getFrameByChannelAndId(0U, 0x100U);
    ASSERT_NE(nullptr, frame);
    EXPECT_EQ(8U, frame->dlc);
    ASSERT_EQ(2U, frame->signals.size());
    EXPECT_EQ("EngineData", frame->frameName);

    SignalConfig const* signal = db.getSignalByName("EngineSpeed");
    ASSERT_NE(nullptr, signal);
    EXPECT_EQ(0x100U, signal->frameId);
    EXPECT_TRUE(signal->isMotorola);
    EXPECT_DOUBLE_EQ(0.25, signal->factor);
    EXPECT_EQ("rpm", signal->unit);
}

/**
 * \desc: The runtime DBC parser reads frames, signals, byte order, scaling,
 * ranges, units and multiplexing information.
 */
TEST(SignalDbTest, load_from_dbc_file)
{
    std::string const dbcPath = "canstack_test_tmp.dbc";
    {
        std::ofstream dbc(dbcPath);
        dbc << "VERSION \"\"\n";
        dbc << "\n";
        dbc << "NS_ :\n";
        dbc << "BS_:\n";
        dbc << "BU_: ECM BCM\n";
        dbc << "\n";
        dbc << "BO_ 256 EngineData: 8 ECM\n";
        dbc << " SG_ EngineSpeed M : 7|16@1+ (0.25,0) [0|8000] \"rpm\" BCM\n";
        dbc << " SG_ VehicleSpeed m1 : 31|8@1+ (0.5,0) [0|250] \"kph\" BCM\n";
        dbc << " SG_ OilTemp m0 : 23|8@1+ (1,-40) [-40|215] \"degC\" BCM\n";
        dbc << " SG_ BatteryVoltage : 7|8@1- (0.1,0) [0|15] \"V\" BCM\n";
        dbc << "\n";
        dbc << "BO_ 512 TargetData: 4 ECM\n";
        dbc << " SG_ TargetSpeed : 0|16@0+ (0.1,0) [0|1000] \"kph\" BCM\n";
    }

    ::canstack::SignalDatabase db;
    ASSERT_TRUE(db.loadFromDbc(dbcPath));

    FrameConfig const* engineFrame = db.getFrameByChannelAndId(0U, 0x100U);
    ASSERT_NE(nullptr, engineFrame);
    EXPECT_EQ("EngineData", engineFrame->frameName);
    EXPECT_EQ(8U, engineFrame->dlc);
    EXPECT_EQ(4U, engineFrame->signals.size());

    SignalConfig const* switchSignal = db.getSignalByName("EngineSpeed");
    ASSERT_NE(nullptr, switchSignal);
    EXPECT_TRUE(switchSignal->isMultiplexerSwitch);
    EXPECT_EQ(0x100U, switchSignal->frameId);

    SignalConfig const* muxed = db.getSignalByName("VehicleSpeed");
    ASSERT_NE(nullptr, muxed);
    EXPECT_EQ(1, muxed->multiplexValue);

    SignalConfig const* oilTemp = db.getSignalByName("OilTemp");
    ASSERT_NE(nullptr, oilTemp);
    EXPECT_EQ(0, oilTemp->multiplexValue);
    EXPECT_DOUBLE_EQ(-40.0, oilTemp->offset);

    SignalConfig const* voltage = db.getSignalByName("BatteryVoltage");
    ASSERT_NE(nullptr, voltage);
    EXPECT_TRUE(voltage->isSigned);
    EXPECT_EQ("V", voltage->unit);

    FrameConfig const* targetFrame = db.getFrameByChannelAndId(0U, 0x200U);
    ASSERT_NE(nullptr, targetFrame);
    ASSERT_EQ(1U, targetFrame->signals.size());
    EXPECT_FALSE(targetFrame->signals[0].isMotorola);

    (void)std::remove(dbcPath.c_str());
}

/**
 * \desc: Missing DBC files are reported as failure.
 */
TEST(SignalDbTest, missing_dbc_file_fails)
{
    ::canstack::SignalDatabase db;

    EXPECT_FALSE(db.loadFromDbc("no_such_file_missing.dbc"));
    EXPECT_EQ(0U, db.getFrameCount());
}

/**
 * \desc: Unknown lookups return nullptr.
 */
TEST(SignalDbTest, unknown_lookups)
{
    ::canstack::SignalDatabase db;

    EXPECT_EQ(nullptr, db.getFrameByChannelAndId(0U, 0x100U));
    EXPECT_EQ(nullptr, db.getSignalByName("Nope"));
}

} // namespace
