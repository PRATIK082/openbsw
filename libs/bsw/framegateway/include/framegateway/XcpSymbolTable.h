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
#include <string>
#include <vector>

namespace framegateway
{

/// XCP measurement/calibration data types.
enum class XcpDataType : uint8_t
{
    UINT8,
    INT8,
    UINT16,
    INT16,
    UINT32,
    INT32,
    FLOAT32,
    FLOAT64
};

/// One calibratable or measurable ECU symbol (typically from A2L).
struct XcpSymbol
{
    std::string name;
    uint32_t address = 0U;
    uint16_t length  = 0U;
    XcpDataType dataType = XcpDataType::UINT8;
    bool isCalibration   = false;
    double min           = 0.0;
    double max           = 0.0;
    std::string unit;
};

/// Name/address lookup over the symbol table.
class XcpSymbolTable
{
public:
    XcpSymbolTable() = default;

    void addSymbol(XcpSymbol const& symbol);
    bool removeSymbol(std::string const& name);
    void clear();

    XcpSymbol const* findByName(std::string const& name) const;
    XcpSymbol const* findByAddress(uint32_t address, uint16_t length) const;

    size_t getSymbolCount() const;

private:
    std::map<std::string, XcpSymbol> m_symbols;
};

/// Parses "UINT8"/"INT16"/"FLOAT32" style names; unknown -> false.
bool parseXcpDataType(std::string const& text, XcpDataType& out);
/// Byte size of a data type.
uint16_t xcpDataTypeSize(XcpDataType type);

} // namespace framegateway
