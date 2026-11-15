/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#pragma once

#include <linknm/LinkManager.h>

namespace ethlink
{

using NmState = linknm::NmState;
using CommMode = linknm::CommMode;
using NmControlBits = linknm::NmControlBits;
using EthLinkManagerConfig = linknm::LinkManagerConfig;
using EthLinkManager = linknm::LinkManager;

inline bool parseNmState(char const* text, NmState& state) { return linknm::parseNmState(text, state); }
inline bool parseState(char const* text, NmState& state) { return linknm::parseNmState(text, state); }

} // namespace ethlink
