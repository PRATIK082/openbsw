/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/GatewayPolicy.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

/**
 * \desc: Frames without a rule are allowed.
 */
TEST(GatewayPolicyTest, default_allow)
{
    ::framegateway::PolicyEngine engine;
    uint8_t type = 0U, ch = 0U;
    uint32_t id  = 0x100U;
    uint8_t data[2U] = {0U, 0U};
    uint16_t len     = 2U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW,
              engine.evaluatePolicy(type, ch, id, data, len, 0U));
    EXPECT_EQ(0U, engine.getPolicyCount());
}

/**
 * \desc: DENY with a source filter blocks non-matching senders only.
 */
TEST(GatewayPolicyTest, deny_with_filter)
{
    ::framegateway::PolicyEngine engine;
    ::framegateway::GatewayPolicy rule{};
    rule.frameId = 0x7DFU;
    rule.action  = ::framegateway::PolicyAction::DENY;
    rule.filter  = [](uint8_t const* data, uint16_t len) {
        return (len > 0U) && (data[0] != 0xF1U);
    };
    engine.addPolicy(rule);

    uint8_t type = 0U, ch = 0U;
    uint32_t id  = 0x7DFU;
    uint8_t bad[1U]  = {0x01U};
    uint8_t good[1U] = {0xF1U};
    uint16_t len     = 1U;
    EXPECT_EQ(::framegateway::PolicyAction::DENY,
              engine.evaluatePolicy(type, ch, id, bad, len, 0U));
    uint32_t id2 = 0x7DFU;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW,
              engine.evaluatePolicy(type, ch, id2, good, len, 0U));
    EXPECT_EQ(1U, engine.getDenyCount());

    EXPECT_TRUE(engine.removePolicy(0U, 0U, 0x7DFU));
    EXPECT_FALSE(engine.removePolicy(0U, 0U, 0x7DFU));
}

/**
 * \desc: TRANSFORM edits the frame in place when the filter passes.
 */
TEST(GatewayPolicyTest, transform)
{
    ::framegateway::PolicyEngine engine;
    ::framegateway::GatewayPolicy rule{};
    rule.frameId   = 0x200U;
    rule.action    = ::framegateway::PolicyAction::TRANSFORM;
    rule.transform = [](uint8_t* data, uint16_t len) {
        for (uint16_t i = 0U; i < len; ++i)
        {
            data[i] = static_cast<uint8_t>(data[i] + 1U);
        }
    };
    engine.addPolicy(rule);

    uint8_t type = 0U, ch = 0U;
    uint32_t id  = 0x200U;
    uint8_t data[2U] = {0x01U, 0xFEU};
    uint16_t len     = 2U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW,
              engine.evaluatePolicy(type, ch, id, data, len, 0U));
    EXPECT_THAT(data, ElementsAre(0x02U, 0xFFU));
}

/**
 * \desc: RATE_LIMIT allows the budget per second, then denies; window resets.
 */
TEST(GatewayPolicyTest, rate_limit)
{
    ::framegateway::PolicyEngine engine;
    ::framegateway::GatewayPolicy rule{};
    rule.frameId       = 0x100U;
    rule.action        = ::framegateway::PolicyAction::RATE_LIMIT;
    rule.maxRatePerSec = 2U;
    engine.addPolicy(rule);

    uint8_t data[1U] = {0U};
    uint16_t len     = 1U;
    uint8_t type = 0U, ch = 0U;
    uint32_t id;
    id = 0x100U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW, engine.evaluatePolicy(type, ch, id, data, len, 0U));
    id = 0x100U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW, engine.evaluatePolicy(type, ch, id, data, len, 0U));
    id = 0x100U;
    EXPECT_EQ(::framegateway::PolicyAction::DENY, engine.evaluatePolicy(type, ch, id, data, len, 0U));

    engine.mainFunction(1000U); // next window
    id = 0x100U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW, engine.evaluatePolicy(type, ch, id, data, len, 1000U));
}

/**
 * \desc: REDIRECT rewrites the frame key; LOG_ONLY calls the hook and allows.
 */
TEST(GatewayPolicyTest, redirect_and_log)
{
    ::framegateway::PolicyEngine engine;
    ::framegateway::GatewayPolicy redirect{};
    redirect.frameId            = 0x300U;
    redirect.action             = ::framegateway::PolicyAction::REDIRECT;
    redirect.redirectChannelType = 2U;
    redirect.redirectFrameId    = 0x400U;
    engine.addPolicy(redirect);

    ::framegateway::GatewayPolicy logRule{};
    logRule.frameId = 0x301U;
    logRule.action  = ::framegateway::PolicyAction::LOG_ONLY;
    engine.addPolicy(logRule);

    uint32_t loggedId = 0U;
    engine.setLogHook([&loggedId](uint32_t id, uint16_t, uint8_t const*) { loggedId = id; });

    uint8_t type = 0U, ch = 0U;
    uint32_t id  = 0x300U;
    uint16_t len = 0U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW,
              engine.evaluatePolicy(type, ch, id, nullptr, len, 0U));
    EXPECT_EQ(2U, type);
    EXPECT_EQ(0x400U, id);

    uint8_t type2 = 0U, ch2 = 0U;
    uint32_t id3 = 0x301U;
    EXPECT_EQ(::framegateway::PolicyAction::ALLOW,
              engine.evaluatePolicy(type2, ch2, id3, nullptr, len, 0U));
    EXPECT_EQ(0x301U, loggedId);
}

} // namespace
