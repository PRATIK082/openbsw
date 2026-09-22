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
#include "framegateway/PduAssembler.h"

#include <cstring>

namespace framegateway
{

bool PduAssembler::packMultiPdu(uint8_t* frameBuffer, uint16_t frameSize,
                                std::vector<PduInFrame> const& pduLayout,
                                PduDataMap const& pduData)
{
    if (frameBuffer == nullptr)
    {
        return false;
    }
    for (auto const& entry : pduLayout)
    {
        if ((entry.startByteOffset + entry.length) > frameSize)
        {
            return false;
        }
        auto it = pduData.find(entry.pduId);
        if (it == pduData.end())
        {
            continue;
        }
        if ((it->second.first < entry.length) || (it->second.second == nullptr))
        {
            return false;
        }
        (void)std::memcpy(frameBuffer + entry.startByteOffset, it->second.second, entry.length);
    }
    return true;
}

PduDataMap PduAssembler::unpackMultiPdu(uint8_t const* frameData, uint16_t frameSize,
                                        std::vector<PduInFrame> const& pduLayout)
{
    PduDataMap result;
    if (frameData == nullptr)
    {
        return result;
    }
    for (auto const& entry : pduLayout)
    {
        if ((entry.startByteOffset + entry.length) > frameSize)
        {
            continue;
        }
        result[entry.pduId] = std::make_pair(entry.length, frameData + entry.startByteOffset);
    }
    return result;
}

uint16_t PduAssembler::extractVariableLengthPdu(uint8_t const* pduData, uint16_t pduAvailable,
                                                uint8_t lengthFieldOffset, uint16_t maxPduLength)
{
    if ((pduData == nullptr) || (lengthFieldOffset >= pduAvailable))
    {
        return 0U;
    }
    uint16_t const length = pduData[lengthFieldOffset];
    return (length > maxPduLength) ? maxPduLength : length;
}

void PduAssembler::applyPadding(uint8_t* frameBuffer, uint16_t usedLength, uint16_t totalLength,
                                uint8_t paddingValue)
{
    if ((frameBuffer == nullptr) || (usedLength >= totalLength))
    {
        return;
    }
    (void)std::memset(frameBuffer + usedLength, paddingValue, totalLength - usedLength);
}

} // namespace framegateway
