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
 * \ingroup ecumode
 */

#include <ecumode/EcuMode.h>

namespace ecumode
{

bool parseEcuMode(std::string const& text, EcuMode& mode)
{
    if (text == "UNINIT")
    {
        mode = EcuMode::UNINIT;
    }
    else if (text == "NORMAL")
    {
        mode = EcuMode::NORMAL;
    }
    else if (text == "STANDBY")
    {
        mode = EcuMode::STANDBY;
    }
    else if (text == "SLEEP")
    {
        mode = EcuMode::SLEEP;
    }
    else if (text == "DIAGNOSTIC")
    {
        mode = EcuMode::DIAGNOSTIC;
    }
    else if (text == "TRANSPORT")
    {
        mode = EcuMode::TRANSPORT;
    }
    else if (text == "MANUFACTURING")
    {
        mode = EcuMode::MANUFACTURING;
    }
    else
    {
        return false;
    }
    return true;
}

bool parseFrameFilterPolicy(std::string const& text, FrameFilterPolicy& policy)
{
    if (text == "ALLOW")
    {
        policy = FrameFilterPolicy::ALLOW;
    }
    else if (text == "BLOCK_RX")
    {
        policy = FrameFilterPolicy::BLOCK_RX;
    }
    else if (text == "BLOCK_TX")
    {
        policy = FrameFilterPolicy::BLOCK_TX;
    }
    else if (text == "BLOCK_BOTH")
    {
        policy = FrameFilterPolicy::BLOCK_BOTH;
    }
    else
    {
        return false;
    }
    return true;
}

} // namespace ecumode
