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

#include <faultmanager/FaultManager.h>

namespace
{

faultmanager::DtcConfig dtc(
    uint32_t id, bool permanent = false, uint8_t aging = 3U, faultmanager::Severity sev = faultmanager::Severity::Major)
{
    faultmanager::DtcConfig c{};
    c.dtc                = id;
    c.format             = faultmanager::Format::Obdii;
    c.severity           = sev;
    c.obdClass           = faultmanager::ObdClass::ClassC;
    c.freezeFrameEnabled = true;
    c.extendedDataSize   = 8U;
    c.agingEnabled       = !permanent;
    c.agingCycles        = aging;
    c.pendingEnabled     = true;
    c.permanent          = permanent;
    return c;
}

} // namespace

TEST(FaultManagerTest, RecordSetsPendingAndConfirmed)
{
    faultmanager::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 1U));
    faultmanager::FaultManager::instance().recordFault(0x010000U);
    uint8_t const st = faultmanager::FaultManager::instance().getDtcStatus(0x010000U);
    EXPECT_NE(st & faultmanager::STATUS_CONFIRMED, 0U);
    EXPECT_NE(st & faultmanager::STATUS_TEST_FAILED, 0U);
    auto stored = faultmanager::FaultManager::instance().getStoredDtcs();
    ASSERT_EQ(stored.size(), 1U);
    EXPECT_EQ(stored[0U], 0x010000U);
}

TEST(FaultManagerTest, FreezeFrameAndExtendedData)
{
    faultmanager::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 1U));
    faultmanager::FreezeSignal sigs[1U] = {};
    sigs[0U].name             = "VehicleSpeed";
    sigs[0U].value            = 88.0F;
    faultmanager::FaultManager::instance().recordFaultWithSignals(0x010000U, faultmanager::Severity::Major, sigs, 1U);
    faultmanager::FreezeFrame ff{};
    ASSERT_TRUE(faultmanager::FaultManager::instance().getFreezeFrame(0x010000U, ff));
    ASSERT_EQ(ff.signals.size(), 1U);
    EXPECT_FLOAT_EQ(ff.signals[0U].value, 88.0F);
    uint8_t ext[4U] = {1U, 2U, 3U, 4U};
    ASSERT_TRUE(faultmanager::FaultManager::instance().storeExtendedData(0x010000U, ext, 4U));
    faultmanager::ExtendedData out{};
    ASSERT_TRUE(faultmanager::FaultManager::instance().getExtendedData(0x010000U, out));
    ASSERT_EQ(out.data.size(), 4U);
    EXPECT_EQ(out.data[2U], 3U);
}

TEST(FaultManagerTest, AgingClearsAfterCycles)
{
    faultmanager::DtcConfig cfgs[1U] = {dtc(0x010000U, false, 2U)};
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 1U));
    faultmanager::FaultManager::instance().recordFault(0x010000U);
    faultmanager::FaultManager::instance().clearFault(0x010000U);
    faultmanager::FaultManager::instance().notifyOperationCycle("WARMUP_CYCLE");
    faultmanager::FaultManager::instance().notifyOperationCycle("WARMUP_CYCLE");
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs().size(), 0U);
}

TEST(FaultManagerTest, PermanentDtcSurvivesClearAll)
{
    faultmanager::DtcConfig cfgs[2U] = {dtc(0x010000U), dtc(0x042000U, true)};
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 2U));
    faultmanager::FaultManager::instance().recordFault(0x010000U);
    faultmanager::FaultManager::instance().recordFault(0x042000U);
    faultmanager::FaultManager::instance().clearAllDtcs();
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs().size(), 1U);
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs()[0U], 0x042000U);
}

TEST(FaultManagerTest, SuppressionBlocksRecording)
{
    faultmanager::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 1U));
    faultmanager::FaultManager::instance().setDtcRecordingSuppressed(true);
    faultmanager::FaultManager::instance().recordFault(0x010000U);
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs().size(), 0U);
    faultmanager::FaultManager::instance().setDtcRecordingSuppressed(false);
    faultmanager::FaultManager::instance().recordFault(0x010000U);
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs().size(), 1U);
}

TEST(FaultManagerTest, SerializeRoundTrip)
{
    faultmanager::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 1U));
    faultmanager::FaultManager::instance().recordFault(0x010000U);
    uint8_t buf[64U] = {};
    etl::span<uint8_t> out(buf, sizeof(buf));
    size_t const n = faultmanager::FaultManager::instance().serialize(out);
    ASSERT_GT(n, 1U);
    ASSERT_TRUE(faultmanager::FaultManager::instance().init(cfgs, 1U));
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs().size(), 0U);
    ASSERT_TRUE(faultmanager::FaultManager::instance().deserialize(etl::span<uint8_t const>(buf, n)));
    EXPECT_EQ(faultmanager::FaultManager::instance().getStoredDtcs().size(), 1U);
}
