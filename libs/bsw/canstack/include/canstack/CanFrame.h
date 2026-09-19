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
 * \ingroup canstack
 */
#pragma once

#include <cstdint>

namespace canstack
{
/**
 * Raw CAN frame value object used throughout the canstack modules.
 *
 * Holds a raw frame identifier (11 bit base or 29 bit extended value), the data
 * length code and up to 8 payload bytes. The class is a plain value type: it
 * copies and compares by content and does not own any resources.
 */
class CanFrame
{
public:
    static uint8_t const MAX_DATA_LENGTH = 8U;
    static uint32_t const MAX_BASE_ID    = 0x7FFU;
    static uint32_t const MAX_EXTENDED_ID = 0x1FFFFFFFU;

    CanFrame();

    /**
     * \param frameId raw frame identifier (11 bit base or 29 bit extended value)
     * \param dlc payload length in bytes (<= MAX_DATA_LENGTH)
     * \param data payload bytes, may be nullptr if dlc == 0
     */
    CanFrame(uint32_t frameId, uint8_t dlc, uint8_t const* data);

    uint32_t getFrameId() const;
    void setFrameId(uint32_t frameId);
    uint8_t getDlc() const;
    void setDlc(uint8_t dlc);
    uint8_t* getData();
    uint8_t const* getData() const;

    /**
     * Replaces dlc and payload.
     * \param data payload bytes, may be nullptr if dlc == 0
     */
    void setData(uint8_t dlc, uint8_t const* data);

    /// \return true if the frame identifier requires the extended (29 bit) format
    bool isExtendedId() const;

    bool operator==(CanFrame const& other) const;
    bool operator!=(CanFrame const& other) const;

private:
    uint32_t m_frameId;
    uint8_t m_dlc;
    uint8_t m_data[MAX_DATA_LENGTH];
};

} // namespace canstack
