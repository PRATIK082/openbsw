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
#include <map>
#include <utility>
#include <vector>

namespace framegateway
{

/// One PDU packed inside a frame at a fixed byte offset.
struct PduInFrame
{
    uint32_t pduId           = 0U;
    uint16_t startByteOffset = 0U;
    uint16_t length          = 0U;
    bool isVariableLength    = false;
    /// Byte offset (relative to PDU start) holding the length, if variable.
    uint8_t lengthFieldOffset = 0U;
};

/// PDU payloads keyed by PDU id: (length, data pointer).
/// Pointers must outlive the pack/unpack call; unpacked pointers alias the
/// frame buffer passed to unpackMultiPdu().
using PduDataMap = std::map<uint32_t, std::pair<uint16_t, uint8_t const*>>;

/**
 * Offset-based multi-PDU frame builder/parser (CAN FD / Ethernet).
 *
 * All functions are stateless and bounds checked; out-of-range layouts are
 * skipped (unpack) or fail the call (pack).
 */
class PduAssembler
{
public:
    PduAssembler() = delete;

    /**
     * Copies every PDU of pduData into frameBuffer at its layout offset.
     * Bytes not covered by any PDU are left untouched (call applyPadding
     * for deterministic fill).
     * \return false when a layout entry exceeds frameSize or a PDU is
     * shorter than its layout length.
     */
    static bool packMultiPdu(uint8_t* frameBuffer, uint16_t frameSize,
                             std::vector<PduInFrame> const& pduLayout,
                             PduDataMap const& pduData);

    /// Extracts PDU views (aliasing frameData) for every in-range entry.
    static PduDataMap unpackMultiPdu(uint8_t const* frameData, uint16_t frameSize,
                                     std::vector<PduInFrame> const& pduLayout);

    /**
     * Reads the length byte of a variable-length PDU.
     * \return length byte clamped to maxPduLength, 0 when out of range.
     */
    static uint16_t extractVariableLengthPdu(uint8_t const* pduData, uint16_t pduAvailable,
                                             uint8_t lengthFieldOffset, uint16_t maxPduLength);

    /// Fills [usedLength, totalLength) with paddingValue; no-op when used >= total.
    static void applyPadding(uint8_t* frameBuffer, uint16_t usedLength, uint16_t totalLength,
                             uint8_t paddingValue = 0xFFU);
};

} // namespace framegateway
