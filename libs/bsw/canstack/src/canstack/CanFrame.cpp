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
#include "canstack/CanFrame.h"
#include "canstack/CanStackLogger.h"

#include <cstring>

namespace canstack
{
namespace logger = ::util::logger;

CanFrame::CanFrame() : CanFrame(0U, 0U, nullptr) {}

CanFrame::CanFrame(uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
: m_frameId(frameId), m_dlc(0U), m_data{}
{
    setData(dlc, data);
}

uint32_t CanFrame::getFrameId() const { return m_frameId; }

void CanFrame::setFrameId(uint32_t const frameId)
{
    if (frameId > MAX_EXTENDED_ID)
    {
        logger::Logger::warn(logger::CANSTACK, "Frame id 0x%lx out of range", frameId);
        return;
    }
    m_frameId = frameId;
}

uint8_t CanFrame::getDlc() const { return m_dlc; }

void CanFrame::setDlc(uint8_t const dlc)
{
    if (dlc > MAX_DATA_LENGTH)
    {
        logger::Logger::warn(logger::CANSTACK, "Dlc %u exceeds frame data length", dlc);
        return;
    }
    m_dlc = dlc;
}

uint8_t* CanFrame::getData() { return m_data; }

uint8_t const* CanFrame::getData() const { return m_data; }

void CanFrame::setData(uint8_t const dlc, uint8_t const* data)
{
    if (dlc > MAX_DATA_LENGTH)
    {
        logger::Logger::warn(logger::CANSTACK, "Dlc %u exceeds frame data length", dlc);
        return;
    }

    m_dlc = dlc;
    if ((data != nullptr) && (dlc > 0U))
    {
        (void)memcpy(m_data, data, static_cast<size_t>(dlc));
    }
}

bool CanFrame::isExtendedId() const { return m_frameId > MAX_BASE_ID; }

bool CanFrame::operator==(CanFrame const& other) const
{
    return (m_frameId == other.m_frameId) && (m_dlc == other.m_dlc)
           && (memcmp(m_data, other.m_data, static_cast<size_t>(m_dlc)) == 0);
}

bool CanFrame::operator!=(CanFrame const& other) const { return !(*this == other); }

} // namespace canstack
