#include <cstring>

#include <gtest/gtest.h>

#include <nvram/Fee.h>
#include <nvram/Nvram.h>

namespace
{

nvram::BlockConfig makeBlock(
    uint16_t id, uint16_t size, bool immediate, nvram::Redundancy red = nvram::Redundancy::Single)
{
    nvram::BlockConfig cfg{};
    cfg.blockId       = id;
    cfg.size          = size;
    cfg.immediateWrite = immediate;
    cfg.redundancy    = red;
    cfg.crcEnabled    = true;
    cfg.crcKind       = nvram::CrcKind::Crc32;
    cfg.storageType   = nvram::StorageType::RamAndFlash;
    cfg.defaultData   = 0x00U;
    cfg.recovery      = nvram::Recovery::UseDefault;
    return cfg;
}

} // namespace

TEST(NvramTest, WriteReadRoundTrip)
{
    nvram::BlockConfig blocks[1U] = {makeBlock(0U, 256U, true)};
    ASSERT_TRUE(nvram::Nvram::instance().init(blocks, 1U));
    uint8_t tx[256U];
    for (uint16_t i = 0U; i < 256U; ++i)
    {
        tx[i] = static_cast<uint8_t>(i);
    }
    EXPECT_EQ(nvram::Nvram::instance().write(0U, tx, 256U), nvram::Result::Ok);
    uint8_t rx[256U] = {};
    EXPECT_EQ(nvram::Nvram::instance().read(0U, rx, 256U), nvram::Result::Ok);
    EXPECT_EQ(std::memcmp(tx, rx, 256U), 0);
}

TEST(NvramTest, CrcErrorDetection)
{
    nvram::BlockConfig blocks[1U] = {makeBlock(0U, 64U, true)};
    ASSERT_TRUE(nvram::Nvram::instance().init(blocks, 1U));
    uint8_t tx[64U] = {};
    ASSERT_EQ(nvram::Nvram::instance().write(0U, tx, 64U), nvram::Result::Ok);
    ASSERT_TRUE(nvram::Nvram::instance().corruptFlashForTest(0U, 10U, 0xFFU));
    uint8_t rx[64U] = {};
    EXPECT_EQ(nvram::Nvram::instance().read(0U, rx, 64U), nvram::Result::CrcError);
}

TEST(NvramTest, ImmediateVsDeferredWrite)
{
    nvram::BlockConfig blocks[2U] = {makeBlock(1U, 8U, true), makeBlock(2U, 8U, false)};
    ASSERT_TRUE(nvram::Nvram::instance().init(blocks, 2U));
    uint8_t v1 = 0x12U;
    uint8_t v2 = 0x34U;
    EXPECT_EQ(nvram::Nvram::instance().write(1U, &v1, 1U), nvram::Result::Ok);
    EXPECT_EQ(nvram::Nvram::instance().write(2U, &v2, 1U), nvram::Result::Ok);
    EXPECT_EQ(nvram::Nvram::instance().shutdown(), nvram::Result::Ok);
    uint8_t r1 = 0U;
    uint8_t r2 = 0U;
    EXPECT_EQ(nvram::Nvram::instance().read(1U, &r1, 1U), nvram::Result::Ok);
    EXPECT_EQ(nvram::Nvram::instance().read(2U, &r2, 1U), nvram::Result::Ok);
    EXPECT_EQ(r1, 0x12U);
    EXPECT_EQ(r2, 0x34U);
}

TEST(NvramTest, DeferredBlockedDuringSleep)
{
    nvram::BlockConfig blocks[1U] = {makeBlock(2U, 8U, false)};
    ASSERT_TRUE(nvram::Nvram::instance().init(blocks, 1U));
    nvram::Nvram::instance().setDeferredWritesBlocked(true);
    uint8_t v = 0xAAU;
    EXPECT_EQ(nvram::Nvram::instance().write(2U, &v, 1U), nvram::Result::DeviceBusy);
    nvram::Nvram::instance().setDeferredWritesBlocked(false);
    EXPECT_EQ(nvram::Nvram::instance().write(2U, &v, 1U), nvram::Result::Ok);
}

TEST(NvramTest, RedundantFallback)
{
    nvram::BlockConfig blocks[1U] = {makeBlock(3U, 32U, true, nvram::Redundancy::Redundant)};
    ASSERT_TRUE(nvram::Nvram::instance().init(blocks, 1U));
    uint8_t tx[32U];
    for (uint8_t i = 0U; i < 32U; ++i)
    {
        tx[i] = i;
    }
    ASSERT_EQ(nvram::Nvram::instance().write(3U, tx, 32U), nvram::Result::Ok);
    // Corrupt copy 0 only: read must fall back to copy 1 and still succeed.
    ASSERT_TRUE(nvram::Nvram::instance().corruptFlashForTest(3U, 5U, 0xFFU));
    uint8_t rx[32U] = {};
    EXPECT_EQ(nvram::Nvram::instance().read(3U, rx, 32U), nvram::Result::Ok);
}

TEST(NvramTest, InvalidBlock)
{
    nvram::BlockConfig blocks[1U] = {makeBlock(0U, 16U, true)};
    ASSERT_TRUE(nvram::Nvram::instance().init(blocks, 1U));
    uint8_t v = 0U;
    EXPECT_EQ(nvram::Nvram::instance().read(99U, &v, 1U), nvram::Result::BlockInvalid);
    EXPECT_EQ(nvram::Nvram::instance().write(99U, &v, 1U), nvram::Result::BlockInvalid);
}

TEST(FeeTest, ReadWriteAndWearSpreads)
{
    nvram::Fee fee(4U, 1024U, 100000U);
    ASSERT_TRUE(fee.init());
    uint8_t tx[16U] = {1U, 2U, 3U};
    ASSERT_TRUE(fee.write(0U, tx, 16U));
    uint8_t rx[16U] = {};
    ASSERT_TRUE(fee.read(0U, rx, 16U));
    EXPECT_EQ(std::memcmp(tx, rx, 16U), 0);
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(fee.write(static_cast<uint32_t>(i * 16), tx, 16U));
    }
    // Sector rotation must keep erase counts balanced.
    uint32_t mn = UINT32_MAX;
    uint32_t mx = 0U;
    for (uint32_t s = 0U; s < 4U; ++s)
    {
        uint32_t const c = fee.getEraseCycleCount(s);
        mn               = (c < mn) ? c : mn;
        mx               = (c > mx) ? c : mx;
    }
    EXPECT_LE(mx - mn, 12U);
    EXPECT_LE(fee.getWearLevel(), 100U);
}

TEST(FeeTest, BadSectorKeepsDataReadable)
{
    nvram::Fee fee(4U, 256U, 100000U);
    ASSERT_TRUE(fee.init());
    uint8_t tx[8U] = {9U, 8U, 7U};
    ASSERT_TRUE(fee.write(0U, tx, 8U));
    ASSERT_TRUE(fee.markBadSector(0U));
    ASSERT_TRUE(fee.write(0U, tx, 8U));
    uint8_t rx[8U] = {};
    EXPECT_TRUE(fee.read(0U, rx, 8U));
    EXPECT_EQ(std::memcmp(tx, rx, 8U), 0);
}
