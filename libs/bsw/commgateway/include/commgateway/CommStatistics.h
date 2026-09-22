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

namespace commgateway
{

/// Cross-protocol communication counters exposed for diagnostics.
struct CommStatistics
{
    uint32_t rxFrameCount    = 0U;
    uint32_t txFrameCount    = 0U;
    uint32_t rxErrorCount    = 0U;
    uint32_t txErrorCount    = 0U;
    uint32_t timeoutCount    = 0U;
    uint32_t gatewayDropCount = 0U;

    void reset();
};

} // namespace commgateway
