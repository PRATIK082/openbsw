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
 * \ingroup integrity
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace integrity
{

/// CRC-8/SAE-J1850 (poly 0x1D, init 0xFF, xor 0xFF) for Integrity P01/P02/P11.
uint8_t crc8SaeJ1850(uint8_t const* data, size_t length);

/// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) for Integrity P05.
uint16_t crc16CcittFalse(uint8_t const* data, size_t length);

/// CRC-32/ISO-HDLC (poly 0xEDB88320, init 0xFFFFFFFF) for Integrity P04.
uint32_t crc32Ieee(uint8_t const* data, size_t length);

} // namespace integrity
