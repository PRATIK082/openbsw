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
#include "commgateway/CommStatistics.h"

namespace commgateway
{

void CommStatistics::reset()
{
    rxFrameCount     = 0U;
    txFrameCount     = 0U;
    rxErrorCount     = 0U;
    txErrorCount     = 0U;
    timeoutCount     = 0U;
    gatewayDropCount = 0U;
}

} // namespace commgateway
