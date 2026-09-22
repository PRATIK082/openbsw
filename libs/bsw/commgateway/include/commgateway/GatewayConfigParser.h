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
 * \ingroup commgateway
 */
#pragma once

#include <cstdint>
#include <string>

namespace commgateway
{

class SignalGateway;
class TimeoutMonitor;
class CommStateManager;

/**
 * Minimal parser for gateway_rules.json (signalRouting, frameTimeouts,
 * channelStates sections). Self-contained (no third-party JSON dependency);
 * accepts the schema documented in doc/gateway.rst.
 */
class GatewayConfigParser
{
public:
    /**
     * Applies the JSON rules to the gateway modules.
     * \return false and fills error on malformed input.
     */
    static bool parse(std::string const& jsonText, SignalGateway& gateway,
                      TimeoutMonitor& timeoutMonitor, CommStateManager& stateManager,
                      std::string& error);
};

} // namespace commgateway
