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
#include "framegateway/XcpSymbolTable.h"

namespace framegateway
{

void XcpSymbolTable::addSymbol(XcpSymbol const& symbol) { m_symbols[symbol.name] = symbol; }

bool XcpSymbolTable::removeSymbol(std::string const& name) { return m_symbols.erase(name) > 0U; }

void XcpSymbolTable::clear() { m_symbols.clear(); }

XcpSymbol const* XcpSymbolTable::findByName(std::string const& name) const
{
    auto it = m_symbols.find(name);
    return (it != m_symbols.end()) ? &it->second : nullptr;
}

XcpSymbol const* XcpSymbolTable::findByAddress(uint32_t address, uint16_t length) const
{
    for (auto const& entry : m_symbols)
    {
        if ((entry.second.address == address) && (entry.second.length == length))
        {
            return &entry.second;
        }
    }
    return nullptr;
}

size_t XcpSymbolTable::getSymbolCount() const { return m_symbols.size(); }

bool parseXcpDataType(std::string const& text, XcpDataType& out)
{
    if (text == "UINT8")
    {
        out = XcpDataType::UINT8;
    }
    else if (text == "INT8")
    {
        out = XcpDataType::INT8;
    }
    else if (text == "UINT16")
    {
        out = XcpDataType::UINT16;
    }
    else if (text == "INT16")
    {
        out = XcpDataType::INT16;
    }
    else if (text == "UINT32")
    {
        out = XcpDataType::UINT32;
    }
    else if (text == "INT32")
    {
        out = XcpDataType::INT32;
    }
    else if (text == "FLOAT32")
    {
        out = XcpDataType::FLOAT32;
    }
    else if (text == "FLOAT64")
    {
        out = XcpDataType::FLOAT64;
    }
    else
    {
        return false;
    }
    return true;
}

uint16_t xcpDataTypeSize(XcpDataType type)
{
    switch (type)
    {
        case XcpDataType::UINT8:
        case XcpDataType::INT8:
            return 1U;
        case XcpDataType::UINT16:
        case XcpDataType::INT16:
            return 2U;
        case XcpDataType::UINT32:
        case XcpDataType::INT32:
        case XcpDataType::FLOAT32:
            return 4U;
        case XcpDataType::FLOAT64:
            return 8U;
        default:
            return 0U;
    }
}

} // namespace framegateway
