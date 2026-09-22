/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * \file
 * \ingroup framegateway
 */
#pragma once

#include <cstdint>
#include <string>

namespace framegateway
{

class FrameGateway;
class TpGateway;
class DiagLink;
class XcpServer;
class PolicyEngine;

/**
 * Minimal parser for frame_gateway_config.json (frameConfigs,
 * diagnosticSessions, xcpSymbols, gatewayPolicies). Self-contained, no
 * third-party JSON dependency; accepts the schema in doc/config.rst.
 *
 * The "filter" string of a policy supports "source == 0xNN" and
 * "source != 0xNN" evaluated against data[0].
 */
class GatewayConfigParser
{
public:
    static bool parse(std::string const& jsonText, FrameGateway& frameGateway,
                      DiagLink& diagLink, XcpServer& xcpServer, PolicyEngine& policyEngine,
                      std::string& error);
};

} // namespace framegateway
