/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/DiagLink.h"
#include "framegateway/FrameGateway.h"
#include "framegateway/GatewayConfigParser.h"
#include "framegateway/GatewayPolicy.h"
#include "framegateway/XcpServer.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

char const* const CONFIG = R"({
  "frameConfigs": [
    {
      "frameId": 1280, "channelType": 0, "channelId": 0,
      "frameLength": 64, "action": "BOTH",
      "pdus": [
        {"pduId": 1001, "startByteOffset": 0, "length": 12, "isVariableLength": false},
        {"pduId": 1002, "startByteOffset": 12, "length": 20, "isVariableLength": false}
      ]
    }
  ],
  "diagnosticSessions": [
    {"channelType": 0, "channelId": 0, "requestFrameId": 2016,
     "responseFrameId": 2024, "pduOffset": 0, "sessionTimeoutMs": 5000}
  ],
  "xcpSymbols": [
    {"name": "EngineSpeed", "address": 536870912, "length": 4, "dataType": "FLOAT32",
     "isCalibration": false, "min": 0, "max": 8000, "unit": "rpm"}
  ],
  "gatewayPolicies": [
    {"frameId": 2015, "channelType": 0, "channelId": 0, "action": "DENY",
     "filter": "source != 0xF1", "maxRatePerSec": 0}
  ]
})";

/**
 * \desc: The example config loads into all four targets.
 */
TEST(GatewayConfigParserTest, parses_full_config)
{
    ::framegateway::FrameGateway frames;
    ::framegateway::DiagLink diag;
    ::framegateway::XcpServer xcp;
    ::framegateway::PolicyEngine policy;

    std::string error;
    ASSERT_TRUE(::framegateway::GatewayConfigParser::parse(CONFIG, frames, diag, xcp, policy,
                                                           error))
        << error;
    EXPECT_EQ(1U, frames.getFrameCount());
    EXPECT_NE(nullptr, frames.findFrame(0U, 0U, 1280U));
    EXPECT_EQ(1U, diag.getSessionCount());
    EXPECT_EQ(1U, policy.getPolicyCount());
}

/**
 * \desc: Schema violations fail with an error and no partial frame table.
 */
TEST(GatewayConfigParserTest, rejects_bad_config)
{
    ::framegateway::FrameGateway frames;
    ::framegateway::DiagLink diag;
    ::framegateway::XcpServer xcp;
    ::framegateway::PolicyEngine policy;

    std::string error;
    EXPECT_FALSE(::framegateway::GatewayConfigParser::parse("{broken", frames, diag, xcp,
                                                            policy, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(::framegateway::GatewayConfigParser::parse(
        R"({"frameConfigs": [{"channelType": 0}]})", frames, diag, xcp, policy, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(0U, frames.getFrameCount());

    error.clear();
    EXPECT_FALSE(::framegateway::GatewayConfigParser::parse(
        R"({"gatewayPolicies": [{"frameId": 1, "action": "BOGUS"}]})", frames, diag, xcp,
        policy, error));
    EXPECT_FALSE(error.empty());
}

} // namespace
