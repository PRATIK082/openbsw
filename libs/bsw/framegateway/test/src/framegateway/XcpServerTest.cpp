/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/XcpServer.h"

#include <gmock/gmock.h>

#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

::framegateway::XcpMemoryRegion makeRegion(uint32_t base, uint16_t size, bool writable)
{
    ::framegateway::XcpMemoryRegion region{};
    region.baseAddress = base;
    region.data        = std::vector<uint8_t>(size, 0U);
    region.writable    = writable;
    return region;
}

::framegateway::XcpSymbol makeSymbol(std::string const& name, uint32_t address, uint16_t length,
                                     bool cal)
{
    ::framegateway::XcpSymbol symbol{};
    symbol.name          = name;
    symbol.address       = address;
    symbol.length        = length;
    symbol.dataType      = ::framegateway::XcpDataType::UINT16;
    symbol.isCalibration = cal;
    return symbol;
}

uint16_t runCmd(::framegateway::XcpServer& server, std::vector<uint8_t> const& cmd,
                uint8_t* response)
{
    uint16_t responseLength = 64U;
    server.onXcpCommandReceived(cmd.data(), static_cast<uint16_t>(cmd.size()), response,
                                responseLength);
    return responseLength;
}

/**
 * \desc: CONNECT/SET_MTA/UPLOAD reads back memory through the MTA.
 */
TEST(XcpServerTest, connect_upload)
{
    ::framegateway::XcpServer server;
    server.init({}, {});
    server.addMemoryRegion(makeRegion(0x20000000U, 64U, true));

    uint8_t response[64U] = {0U};
    EXPECT_EQ(2U, runCmd(server, {::framegateway::XCP_CMD_CONNECT, 0x00U}, response));
    EXPECT_EQ(0xFFU, response[0]);
    EXPECT_EQ(0x00U, response[1]);
    EXPECT_TRUE(server.isConnected());

    // SET_MTA 0x20000000 (little endian), then UPLOAD 4 bytes.
    EXPECT_EQ(2U, runCmd(server,
                         {::framegateway::XCP_CMD_SET_MTA, 0U, 0U, 0U, 0U, 0U, 0U, 0x20U},
                         response));
    EXPECT_EQ(6U, runCmd(server, {::framegateway::XCP_CMD_UPLOAD, 0x04U}, response));
    EXPECT_EQ(0x00U, response[1]);
    EXPECT_EQ(3U, server.getCommandCount());
}

/**
 * \desc: DOWNLOAD writes writable regions; read-only symbols reject writes.
 */
TEST(XcpServerTest, download_and_symbols)
{
    ::framegateway::XcpServer server;
    server.init({makeSymbol("Cal", 0x20000010U, 2U, true),
                 makeSymbol("Meas", 0x20000020U, 2U, false)},
                {});
    server.addMemoryRegion(makeRegion(0x20000000U, 64U, true));

    uint8_t response[64U] = {0U};
    (void)runCmd(server, {::framegateway::XCP_CMD_CONNECT, 0x00U}, response);

    uint8_t calValue[2U] = {0x34U, 0x12U};
    EXPECT_TRUE(server.writeSymbol("Cal", calValue, 2U));
    EXPECT_FALSE(server.writeSymbol("Meas", calValue, 2U)); // read-only
    EXPECT_FALSE(server.writeSymbol("Missing", calValue, 2U));

    uint8_t readBack[2U] = {0U};
    EXPECT_TRUE(server.readSymbol("Cal", readBack, 2U));
    EXPECT_THAT(readBack, ElementsAre(0x34U, 0x12U));
    EXPECT_FALSE(server.readSymbol("Cal", readBack, 4U)); // wrong length

    // Unmapped address fails and counts a memory error.
    EXPECT_FALSE(server.readMemory(0x30000000U, 1U, readBack));
    EXPECT_EQ(2U, server.getMemoryErrorCount());
}

/**
 * \desc: Unknown commands get an error response; commands need a connection.
 */
TEST(XcpServerTest, errors)
{
    ::framegateway::XcpServer server;
    server.init({}, {});

    uint8_t response[64U] = {0U};
    EXPECT_EQ(2U, runCmd(server, {0x99U}, response));
    EXPECT_EQ(::framegateway::XCP_ERR_CMD_UNKNOWN, response[1]);

    EXPECT_EQ(2U, runCmd(server, {::framegateway::XCP_CMD_UPLOAD, 0x01U}, response));
    EXPECT_EQ(::framegateway::XCP_ERR_ACCESS_DENIED, response[1]);
}

/**
 * \desc: Cyclic DAQ lists sample memory and emit DTOs via the transport.
 */
TEST(XcpServerTest, daq_cyclic)
{
    ::framegateway::XcpDaqList daq{};
    daq.daqListId   = 1U;
    daq.elements    = {{0x20000000U, 2U}};
    daq.cycleTimeMs = 10U;
    daq.isRunning   = true;

    ::framegateway::XcpServer server;
    server.init({}, {daq});
    server.addMemoryRegion(makeRegion(0x20000000U, 64U, true));

    std::vector<std::tuple<uint8_t, std::vector<uint8_t>>> dtos;
    server.setTransportHandler(
        [&dtos](uint8_t, uint8_t, uint32_t, uint16_t len, uint8_t const* data) {
            dtos.emplace_back(0U, std::vector<uint8_t>(data, data + len));
        });
    server.setDaqTransport(0U, 0U, 0x700U);

    uint8_t response[64U] = {0U};
    (void)runCmd(server, {::framegateway::XCP_CMD_CONNECT, 0x00U}, response);
    server.mainFunction(9U);
    EXPECT_TRUE(dtos.empty());
    server.mainFunction(10U);
    ASSERT_EQ(1U, dtos.size());
    EXPECT_EQ(3U, std::get<1>(dtos[0]).size()); // daq id + 2 bytes
    EXPECT_EQ(1U, server.getDaqTriggerCount());

    server.triggerDaqList(9U); // unknown list: no-op
    EXPECT_EQ(1U, server.getDaqTriggerCount());
}

/**
 * \desc: Symbol table helpers behave (add/find/remove/parse).
 */
TEST(XcpServerTest, symbol_table)
{
    ::framegateway::XcpSymbolTable table;
    table.addSymbol(makeSymbol("Speed", 0x1000U, 4U, false));
    EXPECT_EQ(1U, table.getSymbolCount());
    EXPECT_NE(nullptr, table.findByName("Speed"));
    EXPECT_EQ(nullptr, table.findByName("Other"));
    EXPECT_NE(nullptr, table.findByAddress(0x1000U, 4U));
    EXPECT_EQ(nullptr, table.findByAddress(0x1000U, 2U));
    EXPECT_TRUE(table.removeSymbol("Speed"));
    EXPECT_EQ(0U, table.getSymbolCount());

    ::framegateway::XcpDataType type = ::framegateway::XcpDataType::UINT8;
    EXPECT_TRUE(::framegateway::parseXcpDataType("FLOAT32", type));
    EXPECT_EQ(::framegateway::XcpDataType::FLOAT32, type);
    EXPECT_FALSE(::framegateway::parseXcpDataType("BOGUS", type));
    EXPECT_EQ(8U, ::framegateway::xcpDataTypeSize(::framegateway::XcpDataType::FLOAT64));
}

} // namespace
