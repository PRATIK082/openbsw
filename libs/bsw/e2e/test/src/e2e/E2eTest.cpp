#include <cstring>

#include <gtest/gtest.h>

#include <e2e/Crc.h>
#include <e2e/E2e.h>

namespace
{

e2e::ProfileConfig profile(uint8_t id, e2e::ProfileType type, uint32_t dataId, uint32_t timeout = 0U)
{
    e2e::ProfileConfig c{};
    c.profileId   = id;
    c.type        = type;
    c.pduId       = 1000U + id;
    c.dataId      = dataId;
    c.counterSize = (type == e2e::ProfileType::P04) ? 16U : 8U;
    if (type == e2e::ProfileType::P01)
    {
        c.counterSize = 4U;
    }
    c.timeoutMs = timeout;
    return c;
}

} // namespace

TEST(E2eCrcTest, KnownVectors)
{
    // "123456789" standard check values.
    uint8_t const msg[9U] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(e2e::crc8SaeJ1850(msg, 9U), 0x4BU);
    EXPECT_EQ(e2e::crc16CcittFalse(msg, 9U), 0x29B1U);
    EXPECT_EQ(e2e::crc32Ieee(msg, 9U), 0xCBF43926UL);
}

TEST(E2eTest, Profile1ProtectCheck)
{
    e2e::ProfileConfig cfgs[1U] = {profile(1U, e2e::ProfileType::P01, 0x1001U)};
    ASSERT_TRUE(e2e::E2e::instance().init(cfgs, 1U));
    uint8_t frame[8U] = {1U, 2U, 3U, 4U, 5U, 6U, 0U, 0U};
    ASSERT_EQ(e2e::E2e::instance().protect(1U, frame, 8U), e2e::CheckResult::Ok);
    EXPECT_EQ(e2e::E2e::instance().check(1U, frame, 8U), e2e::CheckResult::Ok);
    // Replay must fail with counter error.
    EXPECT_EQ(e2e::E2e::instance().check(1U, frame, 8U), e2e::CheckResult::CounterError);
}

TEST(E2eTest, CrcErrorDetected)
{
    e2e::ProfileConfig cfgs[1U] = {profile(11U, e2e::ProfileType::P11, 0x0005U)};
    ASSERT_TRUE(e2e::E2e::instance().init(cfgs, 1U));
    uint8_t frame[8U] = {9U, 9U, 9U, 9U, 9U, 9U, 0U, 0U};
    ASSERT_EQ(e2e::E2e::instance().protect(11U, frame, 8U), e2e::CheckResult::Ok);
    frame[0U] = static_cast<uint8_t>(frame[0U] ^ 0x01U);
    EXPECT_EQ(e2e::E2e::instance().check(11U, frame, 8U), e2e::CheckResult::CrcError);
}

TEST(E2eTest, DataIdErrorDetected)
{
    e2e::ProfileConfig cfgs[2U]
        = {profile(4U, e2e::ProfileType::P04, 0x00000003U),
           profile(5U, e2e::ProfileType::P04, 0x00000004U)};
    ASSERT_TRUE(e2e::E2e::instance().init(cfgs, 2U));
    uint8_t frame[16U] = {};
    ASSERT_EQ(e2e::E2e::instance().protect(4U, frame, 16U), e2e::CheckResult::Ok);
    // Same bytes checked under a different data-ID must be rejected.
    EXPECT_EQ(e2e::E2e::instance().check(5U, frame, 16U), e2e::CheckResult::DataIdError);
}

TEST(E2eTest, Profile5RoundTrip)
{
    e2e::ProfileConfig cfgs[1U] = {profile(6U, e2e::ProfileType::P05, 0x0004U)};
    ASSERT_TRUE(e2e::E2e::instance().init(cfgs, 1U));
    uint8_t f1[8U] = {5U, 6U, 7U, 8U, 9U, 0U, 0U, 0U};
    uint8_t f2[8U] = {5U, 6U, 7U, 8U, 9U, 0U, 0U, 0U};
    ASSERT_EQ(e2e::E2e::instance().protect(6U, f1, 8U), e2e::CheckResult::Ok);
    ASSERT_EQ(e2e::E2e::instance().protect(6U, f2, 8U), e2e::CheckResult::Ok);
    EXPECT_NE(std::memcmp(f1, f2, 8U), 0); // counter advanced
    EXPECT_EQ(e2e::E2e::instance().check(6U, f1, 8U), e2e::CheckResult::Ok);
    EXPECT_EQ(e2e::E2e::instance().check(6U, f2, 8U), e2e::CheckResult::Ok);
}

TEST(E2eTest, Profile2RotatingDataId)
{
    e2e::ProfileConfig cfg = profile(2U, e2e::ProfileType::P02, 0U);
    cfg.dataIdList.push_back(0x01U);
    cfg.dataIdList.push_back(0x02U);
    cfg.dataIdList.push_back(0x03U);
    cfg.dataIdList.push_back(0x04U);
    cfg.counterSize = 4U;
    e2e::ProfileConfig cfgs[1U] = {cfg};
    ASSERT_TRUE(e2e::E2e::instance().init(cfgs, 1U));
    uint8_t frame[6U] = {1U, 2U, 3U, 4U, 0U, 0U};
    ASSERT_EQ(e2e::E2e::instance().protect(2U, frame, 6U), e2e::CheckResult::Ok);
    EXPECT_EQ(e2e::E2e::instance().check(2U, frame, 6U), e2e::CheckResult::Ok);
}
