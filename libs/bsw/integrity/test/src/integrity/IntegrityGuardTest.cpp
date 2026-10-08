#include <cstring>

#include <gtest/gtest.h>

#include <integrity/Crc.h>
#include <integrity/IntegrityGuard.h>

namespace
{

integrity::ProfileConfig profile(uint8_t id, integrity::ProfileType type, uint32_t dataId, uint32_t timeout = 0U)
{
    integrity::ProfileConfig c{};
    c.profileId   = id;
    c.type        = type;
    c.pduId       = 1000U + id;
    c.dataId      = dataId;
    c.counterSize = (type == integrity::ProfileType::P04) ? 16U : 8U;
    if (type == integrity::ProfileType::P01)
    {
        c.counterSize = 4U;
    }
    c.timeoutMs = timeout;
    return c;
}

} // namespace

TEST(IntegrityCrcTest, KnownVectors)
{
    // "123456789" standard check values.
    uint8_t const msg[9U] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(integrity::crc8SaeJ1850(msg, 9U), 0x4BU);
    EXPECT_EQ(integrity::crc16CcittFalse(msg, 9U), 0x29B1U);
    EXPECT_EQ(integrity::crc32Ieee(msg, 9U), 0xCBF43926UL);
}

TEST(IntegrityGuardTest, Profile1ProtectCheck)
{
    integrity::ProfileConfig cfgs[1U] = {profile(1U, integrity::ProfileType::P01, 0x1001U)};
    ASSERT_TRUE(integrity::IntegrityGuard::instance().init(cfgs, 1U));
    uint8_t frame[8U] = {1U, 2U, 3U, 4U, 5U, 6U, 0U, 0U};
    ASSERT_EQ(integrity::IntegrityGuard::instance().protect(1U, frame, 8U), integrity::CheckResult::Ok);
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(1U, frame, 8U), integrity::CheckResult::Ok);
    // Replay must fail with counter error.
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(1U, frame, 8U), integrity::CheckResult::CounterError);
}

TEST(IntegrityGuardTest, CrcErrorDetected)
{
    integrity::ProfileConfig cfgs[1U] = {profile(11U, integrity::ProfileType::P11, 0x0005U)};
    ASSERT_TRUE(integrity::IntegrityGuard::instance().init(cfgs, 1U));
    uint8_t frame[8U] = {9U, 9U, 9U, 9U, 9U, 9U, 0U, 0U};
    ASSERT_EQ(integrity::IntegrityGuard::instance().protect(11U, frame, 8U), integrity::CheckResult::Ok);
    frame[0U] = static_cast<uint8_t>(frame[0U] ^ 0x01U);
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(11U, frame, 8U), integrity::CheckResult::CrcError);
}

TEST(IntegrityGuardTest, DataIdErrorDetected)
{
    integrity::ProfileConfig cfgs[2U]
        = {profile(4U, integrity::ProfileType::P04, 0x00000003U),
           profile(5U, integrity::ProfileType::P04, 0x00000004U)};
    ASSERT_TRUE(integrity::IntegrityGuard::instance().init(cfgs, 2U));
    uint8_t frame[16U] = {};
    ASSERT_EQ(integrity::IntegrityGuard::instance().protect(4U, frame, 16U), integrity::CheckResult::Ok);
    // Same bytes checked under a different data-ID must be rejected.
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(5U, frame, 16U), integrity::CheckResult::DataIdError);
}

TEST(IntegrityGuardTest, Profile5RoundTrip)
{
    integrity::ProfileConfig cfgs[1U] = {profile(6U, integrity::ProfileType::P05, 0x0004U)};
    ASSERT_TRUE(integrity::IntegrityGuard::instance().init(cfgs, 1U));
    uint8_t f1[8U] = {5U, 6U, 7U, 8U, 9U, 0U, 0U, 0U};
    uint8_t f2[8U] = {5U, 6U, 7U, 8U, 9U, 0U, 0U, 0U};
    ASSERT_EQ(integrity::IntegrityGuard::instance().protect(6U, f1, 8U), integrity::CheckResult::Ok);
    ASSERT_EQ(integrity::IntegrityGuard::instance().protect(6U, f2, 8U), integrity::CheckResult::Ok);
    EXPECT_NE(std::memcmp(f1, f2, 8U), 0); // counter advanced
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(6U, f1, 8U), integrity::CheckResult::Ok);
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(6U, f2, 8U), integrity::CheckResult::Ok);
}

TEST(IntegrityGuardTest, Profile2RotatingDataId)
{
    integrity::ProfileConfig cfg = profile(2U, integrity::ProfileType::P02, 0U);
    cfg.dataIdList.push_back(0x01U);
    cfg.dataIdList.push_back(0x02U);
    cfg.dataIdList.push_back(0x03U);
    cfg.dataIdList.push_back(0x04U);
    cfg.counterSize = 4U;
    integrity::ProfileConfig cfgs[1U] = {cfg};
    ASSERT_TRUE(integrity::IntegrityGuard::instance().init(cfgs, 1U));
    uint8_t frame[6U] = {1U, 2U, 3U, 4U, 0U, 0U};
    ASSERT_EQ(integrity::IntegrityGuard::instance().protect(2U, frame, 6U), integrity::CheckResult::Ok);
    EXPECT_EQ(integrity::IntegrityGuard::instance().check(2U, frame, 6U), integrity::CheckResult::Ok);
}
