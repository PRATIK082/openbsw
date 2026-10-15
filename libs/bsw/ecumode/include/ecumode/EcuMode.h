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
#pragma once

#include <cstdint>
#include <string>

namespace ecumode
{

/// ECU-wide communication mode (Phase 5: ECU mode-based communication control).
enum class EcuMode : uint8_t
{
    UNINIT,
    NORMAL,
    STANDBY,
    SLEEP,
    DIAGNOSTIC,
    TRANSPORT,
    MANUFACTURING
};

/// Rx/Tx filter policy applied to a frame in a given ECU mode.
enum class FrameFilterPolicy : uint8_t
{
    ALLOW,
    BLOCK_RX,
    BLOCK_TX,
    BLOCK_BOTH
};

/// Parses an ECU mode name ("NORMAL", ...); returns false on unknown text.
bool parseEcuMode(std::string const& text, EcuMode& mode);

/// Parses a frame filter policy name ("ALLOW", "BLOCK_RX", ...); false on unknown text.
bool parseFrameFilterPolicy(std::string const& text, FrameFilterPolicy& policy);

} // namespace ecumode
