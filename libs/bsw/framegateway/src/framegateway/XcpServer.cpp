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
#include "framegateway/XcpServer.h"

#include <cstring>

namespace framegateway
{
namespace
{

uint8_t const CTO_PID_RESPONSE = 0xFFU;
uint16_t const MAX_CTO_SIZE    = 8U; // Classic CAN CTO; CAN FD uses longer frames.

uint32_t readLittleEndian32(uint8_t const* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8U)
           | (static_cast<uint32_t>(data[2]) << 16U) | (static_cast<uint32_t>(data[3]) << 24U);
}

} // namespace

void XcpServer::init(std::vector<XcpSymbol> const& symbols, std::vector<XcpDaqList> const& daqLists)
{
    clear();
    for (auto const& symbol : symbols)
    {
        m_symbols.addSymbol(symbol);
    }
    for (auto const& daq : daqLists)
    {
        m_daqLists[daq.daqListId] = daq;
    }
    m_initialized = true;
}

void XcpServer::shutdown()
{
    m_initialized = false;
    m_isConnected = false;
    clear();
}

void XcpServer::clear()
{
    m_symbols.clear();
    m_daqLists.clear();
    m_currentMtaAddress = 0U;
    m_commandCount      = 0U;
    m_daqTriggerCount   = 0U;
    m_memoryErrorCount  = 0U;
}

void XcpServer::onXcpCommandReceived(uint8_t const* cmd, uint16_t length, uint8_t* response,
                                     uint16_t& responseLength)
{
    responseLength = 0U;
    if (!m_initialized || (cmd == nullptr) || (response == nullptr) || (length == 0U))
    {
        return;
    }
    m_commandCount++;
    uint8_t const command = cmd[0];
    response[0]           = CTO_PID_RESPONSE;
    response[1]           = XCP_RES_OK;
    responseLength        = 2U;

    switch (command)
    {
        case XCP_CMD_CONNECT:
            m_isConnected = true;
            break;
        case XCP_CMD_DISCONNECT:
            m_isConnected = false;
            break;
        case XCP_CMD_GET_STATUS:
            if (!m_isConnected)
            {
                response[1] = XCP_ERR_ACCESS_DENIED;
                break;
            }
            response[2]  = m_isConnected ? 0x01U : 0x00U;
            responseLength = 3U;
            break;
        case XCP_CMD_GET_ID:
            // Returns a fixed ASCII identifier of this slave.
            response[2]    = 0x01U; // mode: ASCII
            response[3]    = 7U;    // length
            response[4]    = 'O';
            response[5]    = 'p';
            response[6]    = 'e';
            response[7]    = 'n';
            responseLength = 8U;
            break;
        case XCP_CMD_SET_MTA:
            if (length < 8U)
            {
                response[1] = XCP_ERR_OUT_OF_RANGE;
                break;
            }
            m_currentMtaAddress = readLittleEndian32(cmd + 4U);
            break;
        case XCP_CMD_UPLOAD:
        {
            if ((length < 2U) || !m_isConnected)
            {
                response[1] = XCP_ERR_ACCESS_DENIED;
                break;
            }
            uint8_t const count = cmd[1];
            if (!readMemory(m_currentMtaAddress, count, response + 2U))
            {
                response[1]   = XCP_ERR_OUT_OF_RANGE;
                responseLength = 2U;
                break;
            }
            responseLength = static_cast<uint16_t>(2U + count);
            m_currentMtaAddress += count;
            break;
        }
        case XCP_CMD_DOWNLOAD:
        {
            if ((length < 2U) || !m_isConnected)
            {
                response[1] = XCP_ERR_ACCESS_DENIED;
                break;
            }
            uint8_t const count = cmd[1];
            if ((static_cast<uint16_t>(2U + count) > length)
                || !writeMemory(m_currentMtaAddress, count, cmd + 2U))
            {
                response[1]   = XCP_ERR_OUT_OF_RANGE;
                responseLength = 2U;
                break;
            }
            m_currentMtaAddress += count;
            break;
        }
        case XCP_CMD_BUILD_CHECKSUM:
        {
            if (length < 8U)
            {
                response[1] = XCP_ERR_OUT_OF_RANGE;
                break;
            }
            uint32_t const blockSize = readLittleEndian32(cmd + 4U);
            uint32_t checksum        = 0U;
            for (uint32_t i = 0U; i < blockSize; ++i)
            {
                uint8_t byte = 0U;
                if (!readMemory(m_currentMtaAddress + i, 1U, &byte))
                {
                    response[1]   = XCP_ERR_OUT_OF_RANGE;
                    responseLength = 2U;
                    break;
                }
                checksum += byte;
            }
            if (response[1] == XCP_RES_OK)
            {
                response[2]    = 0x00U; // checksum type: additive
                response[3]    = static_cast<uint8_t>((checksum >> 24U) & 0xFFU);
                response[4]    = static_cast<uint8_t>((checksum >> 16U) & 0xFFU);
                response[5]    = static_cast<uint8_t>((checksum >> 8U) & 0xFFU);
                response[6]    = static_cast<uint8_t>(checksum & 0xFFU);
                responseLength = 7U;
            }
            break;
        }
        case XCP_CMD_SET_DAQ_PTR:
        case XCP_CMD_WRITE_DAQ:
        case XCP_CMD_START_STOP_DAQ:
        case XCP_CMD_START_STOP_SYNCH:
            // DAQ configuration is code-first; runtime reconfiguration is
            // acknowledged without effect (documented simplification).
            if (!m_isConnected)
            {
                response[1] = XCP_ERR_ACCESS_DENIED;
            }
            break;
        default:
            response[1] = XCP_ERR_CMD_UNKNOWN;
            break;
    }
    (void)MAX_CTO_SIZE;
}

bool XcpServer::readMemory(uint32_t address, uint16_t length, uint8_t* data) const
{
    if ((data == nullptr) && (length > 0U))
    {
        return false;
    }
    uint8_t const* src = resolveAddress(address, length);
    if (src == nullptr)
    {
        return false;
    }
    (void)std::memcpy(data, src, length);
    return true;
}

bool XcpServer::writeMemory(uint32_t address, uint16_t length, uint8_t const* data)
{
    if ((data == nullptr) && (length > 0U))
    {
        return false;
    }
    uint8_t* dst = resolveAddress(address, length);
    if (dst == nullptr)
    {
        const_cast<XcpServer*>(this)->m_memoryErrorCount++;
        return false;
    }
    (void)std::memcpy(dst, data, length);
    return true;
}

bool XcpServer::readSymbol(std::string const& name, uint8_t* data, uint16_t length) const
{
    XcpSymbol const* symbol = m_symbols.findByName(name);
    if ((symbol == nullptr) || (length != symbol->length))
    {
        return false;
    }
    return readMemory(symbol->address, length, data);
}

bool XcpServer::writeSymbol(std::string const& name, uint8_t const* data, uint16_t length)
{
    XcpSymbol const* symbol = m_symbols.findByName(name);
    if ((symbol == nullptr) || (length != symbol->length) || !symbol->isCalibration)
    {
        m_memoryErrorCount++;
        return false;
    }
    return writeMemory(symbol->address, length, data);
}

void XcpServer::addMemoryRegion(XcpMemoryRegion const& region) { m_memory.push_back(region); }

void XcpServer::triggerDaqList(uint8_t daqListId)
{
    auto it = m_daqLists.find(daqListId);
    if (it == m_daqLists.end())
    {
        return;
    }
    std::vector<uint8_t> dto;
    dto.push_back(daqListId);
    for (auto const& element : it->second.elements)
    {
        std::vector<uint8_t> sample(element.second, 0U);
        if (!readMemory(element.first, element.second, sample.data()))
        {
            m_memoryErrorCount++;
            return;
        }
        dto.insert(dto.end(), sample.begin(), sample.end());
    }
    m_daqTriggerCount++;
    sendDaqDto(daqListId, dto);
}

void XcpServer::mainFunction(uint32_t nowMs)
{
    if (!m_initialized || !m_isConnected)
    {
        return;
    }
    for (auto& entry : m_daqLists)
    {
        XcpDaqList& daq = entry.second;
        if (!daq.isRunning || (daq.cycleTimeMs == 0U))
        {
            continue;
        }
        if ((nowMs - daq.lastTriggerTimeMs) >= daq.cycleTimeMs)
        {
            daq.lastTriggerTimeMs = nowMs;
            triggerDaqList(daq.daqListId);
        }
    }
}

void XcpServer::setTransportHandler(XcpTransportHandler tx) { m_txHandler = tx; }

void XcpServer::setDaqTransport(uint8_t channelType, uint8_t channelId, uint32_t frameId)
{
    m_daqChannelType = channelType;
    m_daqChannelId   = channelId;
    m_daqFrameId     = frameId;
}

bool XcpServer::isConnected() const { return m_isConnected; }

uint32_t XcpServer::getCommandCount() const { return m_commandCount; }

uint32_t XcpServer::getDaqTriggerCount() const { return m_daqTriggerCount; }

uint32_t XcpServer::getMemoryErrorCount() const { return m_memoryErrorCount; }

uint8_t const* XcpServer::resolveAddress(uint32_t address, uint16_t length) const
{
    for (auto const& region : m_memory)
    {
        if ((address >= region.baseAddress)
            && ((address + length) <= (region.baseAddress + region.data.size())))
        {
            return region.data.data() + (address - region.baseAddress);
        }
    }
    return nullptr;
}

uint8_t* XcpServer::resolveAddress(uint32_t address, uint16_t length)
{
    for (auto& region : m_memory)
    {
        if ((address >= region.baseAddress)
            && ((address + length) <= (region.baseAddress + region.data.size()))
            && region.writable)
        {
            return region.data.data() + (address - region.baseAddress);
        }
    }
    return nullptr;
}

void XcpServer::sendDaqDto(uint8_t daqListId, std::vector<uint8_t> const& dto)
{
    if (m_txHandler && !dto.empty())
    {
        m_txHandler(m_daqChannelType, m_daqChannelId, m_daqFrameId,
                    static_cast<uint16_t>(dto.size()), dto.data());
    }
}

} // namespace framegateway
