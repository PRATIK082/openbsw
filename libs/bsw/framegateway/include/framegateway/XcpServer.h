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

#include "framegateway/XcpSymbolTable.h"

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace framegateway
{

/// XCP command codes (subset of ASAM MCD-1 XCP).
static uint8_t const XCP_CMD_CONNECT       = 0xFFU;
static uint8_t const XCP_CMD_DISCONNECT    = 0xFEU;
static uint8_t const XCP_CMD_GET_STATUS    = 0xFDU;
static uint8_t const XCP_CMD_GET_ID        = 0xFAU;
static uint8_t const XCP_CMD_SET_MTA       = 0xF6U;
static uint8_t const XCP_CMD_UPLOAD        = 0xF5U;
static uint8_t const XCP_CMD_DOWNLOAD      = 0xF0U;
static uint8_t const XCP_CMD_BUILD_CHECKSUM = 0xF3U;
static uint8_t const XCP_CMD_SET_DAQ_PTR   = 0xE2U;
static uint8_t const XCP_CMD_WRITE_DAQ     = 0xE1U;
static uint8_t const XCP_CMD_START_STOP_DAQ = 0xDEU;
static uint8_t const XCP_CMD_START_STOP_SYNCH = 0xDDU;

/// XCP positive response PID and generic error code (simplified CTO format:
// PID 0xFF, return code, payload...).
static uint8_t const XCP_RES_OK    = 0x00U;
static uint8_t const XCP_ERR_CMD_UNKNOWN = 0x20U;
static uint8_t const XCP_ERR_OUT_OF_RANGE = 0x22U;
static uint8_t const XCP_ERR_ACCESS_DENIED = 0x25U;

/// One DAQ list: cyclic or event-triggered measurement elements.
struct XcpDaqList
{
    uint8_t daqListId = 0U;
    /// (memory address, length) elements sampled in order.
    std::vector<std::pair<uint32_t, uint16_t>> elements;
    /// Cycle period; 0 = event-triggered only.
    uint32_t cycleTimeMs     = 0U;
    uint32_t lastTriggerTimeMs = 0U;
    bool isRunning           = false;
};

/// Simulated ECU memory region backing MTA accesses.
struct XcpMemoryRegion
{
    uint32_t baseAddress = 0U;
    std::vector<uint8_t> data;
    bool writable        = false;
};

/// DAQ DTO transmit hook: (daqListId, length, data).
using XcpDaqSender = std::function<void(uint8_t, uint16_t, uint8_t const*)>;
/// CTO transmit hook: (channelType, channelId, frameId, length, data).
using XcpTransportHandler
    = std::function<void(uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*)>;

/**
 * XCP slave: calibration (DOWNLOAD/UPLOAD via MTA) and measurement
 * (DAQ lists, cyclic or event-triggered).
 *
 * Memory accesses are bounds checked against registered regions; symbol
 * writes additionally honor isCalibration and min/max range. DAQ DTOs go
 * through the transport handler with a configured frame key.
 */
class XcpServer
{
public:
    XcpServer() = default;

    void init(std::vector<XcpSymbol> const& symbols, std::vector<XcpDaqList> const& daqLists);
    void shutdown();
    void clear();

    void onXcpCommandReceived(uint8_t const* cmd, uint16_t length, uint8_t* response,
                              uint16_t& responseLength);

    bool readMemory(uint32_t address, uint16_t length, uint8_t* data) const;
    bool writeMemory(uint32_t address, uint16_t length, uint8_t const* data);
    bool readSymbol(std::string const& name, uint8_t* data, uint16_t length) const;
    bool writeSymbol(std::string const& name, uint8_t const* data, uint16_t length);
    void addMemoryRegion(XcpMemoryRegion const& region);

    void triggerDaqList(uint8_t daqListId);
    /// Samples due cyclic DAQ lists; call every millisecond.
    void mainFunction(uint32_t nowMs);

    void setTransportHandler(XcpTransportHandler tx);
    void setDaqTransport(uint8_t channelType, uint8_t channelId, uint32_t frameId);

    bool isConnected() const;
    uint32_t getCommandCount() const;
    uint32_t getDaqTriggerCount() const;
    uint32_t getMemoryErrorCount() const;

private:
    uint8_t const* resolveAddress(uint32_t address, uint16_t length) const;
    uint8_t* resolveAddress(uint32_t address, uint16_t length);
    void sendDaqDto(uint8_t daqListId, std::vector<uint8_t> const& dto);

    XcpSymbolTable m_symbols;
    std::map<uint8_t, XcpDaqList> m_daqLists;
    std::vector<XcpMemoryRegion> m_memory;
    uint32_t m_currentMtaAddress = 0U;
    bool m_isConnected           = false;
    XcpTransportHandler m_txHandler;
    uint8_t m_daqChannelType = 0U;
    uint8_t m_daqChannelId   = 0U;
    uint32_t m_daqFrameId    = 0U;
    uint32_t m_commandCount  = 0U;
    uint32_t m_daqTriggerCount = 0U;
    uint32_t m_memoryErrorCount = 0U;
    bool m_initialized       = false;
};

} // namespace framegateway
