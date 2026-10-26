/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <integrity/Crc.h>

namespace integrity
{

uint8_t crc8SaeJ1850(uint8_t const* data, size_t length)
{
    uint8_t crc = 0xFFU;
    for (size_t i = 0U; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t b = 0U; b < 8U; ++b)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x1DU);
            }
            else
            {
                crc = static_cast<uint8_t>(crc << 1);
            }
        }
    }
    return static_cast<uint8_t>(crc ^ 0xFFU);
}

uint16_t crc16CcittFalse(uint8_t const* data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0U; i < length; ++i)
    {
        crc ^= static_cast<uint16_t>(static_cast<uint16_t>(data[i]) << 8U);
        for (uint8_t b = 0U; b < 8U; ++b)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

uint32_t crc32Ieee(uint8_t const* data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0U; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t b = 0U; b < 8U; ++b)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

} // namespace integrity
