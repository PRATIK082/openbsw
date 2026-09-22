/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/CommStateManager.h"
#include "commgateway/GatewayConfigParser.h"
#include "commgateway/SignalGateway.h"
#include "commgateway/TimeoutMonitor.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

char const* const RULES = R"({
  "signalRouting": [
    {
      "signalName": "VehicleSpeed",
      "destinations": [
        {"channelType": 1, "channelId": 0},
        {"channelType": 2, "channelId": 0}
      ],
      "timeoutMs": 500,
      "minIntervalMs": 100
    },
    {
      "signalName": "EngineSpeed",
      "destinations": [{"channelType": 1, "channelId": 0}],
      "transform": {"scale": 0.25, "offset": 0}
    }
  ],
  "frameTimeouts": [
    {"channelType": 0, "channelId": 0, "frameId": 256, "timeoutMs": 200}
  ],
  "channelStates": [
    {"channelType": 0, "channelId": 0, "autoRecovery": true, "recoveryTimeMs": 1000}
  ]
})";

/**
 * \desc: The example rules load into gateway, timeouts and channel states.
 */
TEST(GatewayConfigParserTest, parses_example_rules)
{
    ::commgateway::SignalGateway gateway;
    ::commgateway::TimeoutMonitor timeouts;
    ::commgateway::CommStateManager states;
    gateway.init(nullptr);
    timeouts.init();
    states.init();

    std::string error;
    ASSERT_TRUE(::commgateway::GatewayConfigParser::parse(RULES, gateway, timeouts, states, error))
        << error;
    EXPECT_EQ(2U, gateway.getRuleCount());
    EXPECT_EQ(2U, timeouts.getEntryCount()); // 1 signal + 1 frame timeout
    EXPECT_EQ(1U, states.getChannelCount());
}

/**
 * \desc: Malformed input fails with an error instead of partial state.
 */
TEST(GatewayConfigParserTest, rejects_malformed_input)
{
    ::commgateway::SignalGateway gateway;
    ::commgateway::TimeoutMonitor timeouts;
    ::commgateway::CommStateManager states;
    gateway.init(nullptr);
    timeouts.init();
    states.init();

    std::string error;
    EXPECT_FALSE(::commgateway::GatewayConfigParser::parse("{not json", gateway, timeouts, states,
                                                           error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(::commgateway::GatewayConfigParser::parse(
        R"({"signalRouting": [{"destinations": []}]})", gateway, timeouts, states, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(0U, gateway.getRuleCount());
}

} // namespace
