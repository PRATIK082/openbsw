/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/SignalGateway.h"

#include <gmock/gmock.h>

#include <cmath>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

/**
 * \desc: Updates without a rule are ignored; ruled signals reach the sender.
 */
TEST(SignalGatewayTest, basic_routing)
{
    ::commgateway::SignalGateway gateway;
    gateway.init(nullptr);

    std::vector<std::tuple<uint8_t, uint32_t, std::string, double>> sent;
    gateway.setChannelSender(
        [&sent](uint8_t type, uint32_t id, std::string const& signal, double value) {
            sent.emplace_back(type, id, signal, value);
            return true;
        });

    gateway.onSignalUpdate("Unknown", 1.0, 0U);
    EXPECT_TRUE(sent.empty());

    ::commgateway::SignalRoutingRule rule{};
    rule.signalName   = "VehicleSpeed";
    rule.destinations = {{::commgateway::CHANNEL_TYPE_LIN, 0U},
                         {::commgateway::CHANNEL_TYPE_ETH, 0U}};
    gateway.registerRoutingRule(rule);
    EXPECT_EQ(1U, gateway.getRuleCount());

    gateway.onSignalUpdate("VehicleSpeed", 50.0, 0U);
    ASSERT_EQ(2U, sent.size());
    EXPECT_EQ(::commgateway::CHANNEL_TYPE_LIN, std::get<0>(sent[0]));
    EXPECT_EQ(::commgateway::CHANNEL_TYPE_ETH, std::get<0>(sent[1]));
    EXPECT_EQ(2U, gateway.getForwardCount());
}

/**
 * \desc: Transforms apply; NaN results drop the signal.
 */
TEST(SignalGatewayTest, transform_and_nan_drop)
{
    ::commgateway::SignalGateway gateway;
    gateway.init(nullptr);

    std::vector<double> sent;
    gateway.setChannelSender([&sent](uint8_t, uint32_t, std::string const&, double value) {
        sent.push_back(value);
        return true;
    });

    ::commgateway::SignalRoutingRule scaled{};
    scaled.signalName   = "Scaled";
    scaled.destinations = {{0U, 0U}};
    scaled.transform    = [](double v) { return (v * 0.25) + 1.0; };
    gateway.registerRoutingRule(scaled);
    gateway.onSignalUpdate("Scaled", 8.0, 0U);
    ASSERT_EQ(1U, sent.size());
    EXPECT_DOUBLE_EQ(3.0, sent[0]);

    ::commgateway::SignalRoutingRule dropped{};
    dropped.signalName   = "Dropped";
    dropped.destinations = {{0U, 0U}};
    dropped.transform    = [](double) { return std::numeric_limits<double>::quiet_NaN(); };
    gateway.registerRoutingRule(dropped);
    gateway.onSignalUpdate("Dropped", 1.0, 0U);
    EXPECT_EQ(1U, sent.size());
    EXPECT_EQ(1U, gateway.getDropCount());
}

/**
 * \desc: Rate-limited updates queue up and flush via mainFunction().
 */
TEST(SignalGatewayTest, rate_limiting)
{
    ::commgateway::SignalGateway gateway;
    gateway.init(nullptr);

    std::vector<double> sent;
    gateway.setChannelSender([&sent](uint8_t, uint32_t, std::string const&, double value) {
        sent.push_back(value);
        return true;
    });

    ::commgateway::SignalRoutingRule rule{};
    rule.signalName   = "Fast";
    rule.destinations = {{0U, 0U}};
    rule.minIntervalMs = 100U;
    gateway.registerRoutingRule(rule);

    gateway.onSignalUpdate("Fast", 1.0, 0U);
    gateway.onSignalUpdate("Fast", 2.0, 10U);
    ASSERT_EQ(1U, sent.size());
    EXPECT_EQ(1U, gateway.getPendingCount());

    gateway.mainFunction(50U);
    EXPECT_EQ(1U, sent.size());
    gateway.mainFunction(100U);
    ASSERT_EQ(2U, sent.size());
    EXPECT_DOUBLE_EQ(2.0, sent[1]);
    EXPECT_EQ(0U, gateway.getPendingCount());
}

} // namespace
