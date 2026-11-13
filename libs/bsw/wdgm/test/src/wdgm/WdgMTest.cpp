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

#include <wdgm/WdgM.h>

namespace
{

wdgm::EntityConfig makeEntity(uint32_t id, uint32_t aliveMin = 1U, uint32_t aliveMax = 1U, bool qa = false)
{
    wdgm::EntityConfig c{};
    c.id        = id;
    c.aliveMin  = aliveMin;
    c.aliveMax  = aliveMax;
    c.qaEnabled = qa;
    return c;
}

} // namespace

TEST(WdgMTest, AlivePass)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig cfgs[1U] = {makeEntity(1U)};
    ASSERT_TRUE(w.init(cfgs, 1U));
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    w.checkpointReached(1U);
    w.mainFunction();
    EXPECT_EQ(cbCount, 0);
    EXPECT_FALSE(w.hasFailure());
    EXPECT_FALSE(w.isEntityFailed(1U));
}

TEST(WdgMTest, AliveFailTooFew)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig cfgs[1U] = {makeEntity(1U)};
    ASSERT_TRUE(w.init(cfgs, 1U));
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    w.mainFunction();
    EXPECT_EQ(cbCount, 1);
    EXPECT_TRUE(w.hasFailure());
    EXPECT_TRUE(w.isEntityFailed(1U));
}

TEST(WdgMTest, AliveFailTooMany)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig cfgs[1U] = {makeEntity(1U)};
    ASSERT_TRUE(w.init(cfgs, 1U));
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    w.checkpointReached(1U);
    w.checkpointReached(1U);
    w.mainFunction();
    EXPECT_EQ(cbCount, 1);
    EXPECT_TRUE(w.hasFailure());
}

TEST(WdgMTest, WindowViolation)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig c = makeEntity(1U, 2U, 2U);
    c.windowMinMs = 10U;
    c.windowMaxMs = 100U;
    wdgm::EntityConfig cfgs[1U] = {c};
    ASSERT_TRUE(w.init(cfgs, 1U));
    int t = 0;
    w.setTickProvider([&]() { return static_cast<uint32_t>(t); });
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    t = 0;
    w.checkpointReached(1U);
    t = 5;
    w.checkpointReached(1U); // too soon -> window violation
    w.mainFunction();
    EXPECT_EQ(cbCount, 1);
    EXPECT_TRUE(w.hasFailure());
}

TEST(WdgMTest, QaMismatch)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig c = makeEntity(1U, 1U, 1U, true);
    wdgm::EntityConfig cfgs[1U] = {c};
    ASSERT_TRUE(w.init(cfgs, 1U));
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    uint32_t ch = w.getChallenge(1U);
    EXPECT_FALSE(w.checkResponse(1U, ch + 1U));
    w.checkpointReached(1U);
    w.mainFunction();
    EXPECT_EQ(cbCount, 1);
    EXPECT_TRUE(w.hasFailure());
}

TEST(WdgMTest, QaPass)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig c = makeEntity(1U, 1U, 1U, true);
    wdgm::EntityConfig cfgs[1U] = {c};
    ASSERT_TRUE(w.init(cfgs, 1U));
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    uint32_t ch = w.getChallenge(1U);
    EXPECT_TRUE(w.checkResponse(1U, ch));
    w.checkpointReached(1U);
    w.mainFunction();
    EXPECT_EQ(cbCount, 0);
    EXPECT_FALSE(w.hasFailure());
}

TEST(WdgMTest, QaCustomVerifierMismatch)
{
    auto& w = wdgm::WdgM::instance();
    wdgm::EntityConfig c = makeEntity(1U, 1U, 1U, true);
    wdgm::EntityConfig cfgs[1U] = {c};
    ASSERT_TRUE(w.init(cfgs, 1U));
    w.setQaVerifier(1U, [](uint32_t challenge, uint32_t response) {
        return response == (challenge * 2U);
    });
    int cbCount = 0;
    w.setSafeStateCallback([&]() { ++cbCount; });
    uint32_t ch = w.getChallenge(1U);
    w.checkResponse(1U, ch * 2U + 1U);
    w.checkpointReached(1U);
    w.mainFunction();
    EXPECT_EQ(cbCount, 1);
}