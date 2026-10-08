#include <gtest/gtest.h>

#include <dem/Dem.h>

namespace
{

dem::DtcConfig dtc(
    uint32_t id, bool permanent = false, uint8_t aging = 3U, dem::Severity sev = dem::Severity::Major)
{
    dem::DtcConfig c{};
    c.dtc                = id;
    c.format             = dem::Format::Obdii;
    c.severity           = sev;
    c.obdClass           = dem::ObdClass::ClassC;
    c.freezeFrameEnabled = true;
    c.extendedDataSize   = 8U;
    c.agingEnabled       = !permanent;
    c.agingCycles        = aging;
    c.pendingEnabled     = true;
    c.permanent          = permanent;
    return c;
}

} // namespace

TEST(DemTest, RecordSetsPendingAndConfirmed)
{
    dem::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 1U));
    dem::Dem::instance().recordFault(0x010000U);
    uint8_t const st = dem::Dem::instance().getDtcStatus(0x010000U);
    EXPECT_NE(st & dem::STATUS_CONFIRMED, 0U);
    EXPECT_NE(st & dem::STATUS_TEST_FAILED, 0U);
    auto stored = dem::Dem::instance().getStoredDtcs();
    ASSERT_EQ(stored.size(), 1U);
    EXPECT_EQ(stored[0U], 0x010000U);
}

TEST(DemTest, FreezeFrameAndExtendedData)
{
    dem::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 1U));
    dem::FreezeSignal sigs[1U] = {};
    sigs[0U].name             = "VehicleSpeed";
    sigs[0U].value            = 88.0F;
    dem::Dem::instance().recordFaultWithSignals(0x010000U, dem::Severity::Major, sigs, 1U);
    dem::FreezeFrame ff{};
    ASSERT_TRUE(dem::Dem::instance().getFreezeFrame(0x010000U, ff));
    ASSERT_EQ(ff.signals.size(), 1U);
    EXPECT_FLOAT_EQ(ff.signals[0U].value, 88.0F);
    uint8_t ext[4U] = {1U, 2U, 3U, 4U};
    ASSERT_TRUE(dem::Dem::instance().storeExtendedData(0x010000U, ext, 4U));
    dem::ExtendedData out{};
    ASSERT_TRUE(dem::Dem::instance().getExtendedData(0x010000U, out));
    ASSERT_EQ(out.data.size(), 4U);
    EXPECT_EQ(out.data[2U], 3U);
}

TEST(DemTest, AgingClearsAfterCycles)
{
    dem::DtcConfig cfgs[1U] = {dtc(0x010000U, false, 2U)};
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 1U));
    dem::Dem::instance().recordFault(0x010000U);
    dem::Dem::instance().clearFault(0x010000U);
    dem::Dem::instance().notifyOperationCycle("WARMUP_CYCLE");
    dem::Dem::instance().notifyOperationCycle("WARMUP_CYCLE");
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs().size(), 0U);
}

TEST(DemTest, PermanentDtcSurvivesClearAll)
{
    dem::DtcConfig cfgs[2U] = {dtc(0x010000U), dtc(0x042000U, true)};
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 2U));
    dem::Dem::instance().recordFault(0x010000U);
    dem::Dem::instance().recordFault(0x042000U);
    dem::Dem::instance().clearAllDtcs();
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs().size(), 1U);
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs()[0U], 0x042000U);
}

TEST(DemTest, SuppressionBlocksRecording)
{
    dem::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 1U));
    dem::Dem::instance().setDtcRecordingSuppressed(true);
    dem::Dem::instance().recordFault(0x010000U);
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs().size(), 0U);
    dem::Dem::instance().setDtcRecordingSuppressed(false);
    dem::Dem::instance().recordFault(0x010000U);
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs().size(), 1U);
}

TEST(DemTest, SerializeRoundTrip)
{
    dem::DtcConfig cfgs[1U] = {dtc(0x010000U)};
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 1U));
    dem::Dem::instance().recordFault(0x010000U);
    uint8_t buf[64U] = {};
    etl::span<uint8_t> out(buf, sizeof(buf));
    size_t const n = dem::Dem::instance().serialize(out);
    ASSERT_GT(n, 1U);
    ASSERT_TRUE(dem::Dem::instance().init(cfgs, 1U));
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs().size(), 0U);
    ASSERT_TRUE(dem::Dem::instance().deserialize(etl::span<uint8_t const>(buf, n)));
    EXPECT_EQ(dem::Dem::instance().getStoredDtcs().size(), 1U);
}
