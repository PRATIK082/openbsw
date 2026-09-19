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
#include "canstack/CanHwMcp2515.h"
#include "canstack/CanFrame.h"
#include "canstack/CanStackLogger.h"

namespace canstack
{
namespace logger = ::util::logger;

CanHwMcp2515::CanHwMcp2515(IMcp2515Bus& bus, uint32_t const oscillatorHz)
: m_bus(bus), m_oscillatorHz(oscillatorHz)
{
}

bool CanHwMcp2515::init(uint8_t const channelId, uint32_t const baudrate)
{
    uint8_t cnf1 = 0U;
    uint8_t cnf2 = 0U;
    uint8_t cnf3 = 0U;

    if (!computeBaudConfig(baudrate, cnf1, cnf2, cnf3))
    {
        logger::Logger::error(logger::CANSTACK, "MCP2515: baudrate %lu not reachable", baudrate);
        return false;
    }

    // Reset the chip and wait for configuration mode.
    uint8_t const resetInstr = INSTR_RESET;
    (void)m_bus.transfer(&resetInstr, 1U, nullptr, 0U);

    if (!setMode(MODE_CONFIG))
    {
        logger::Logger::error(logger::CANSTACK, "MCP2515: config mode not reached");
        return false;
    }

    if (!writeRegister(REG_CNF1, cnf1) || !writeRegister(REG_CNF2, cnf2)
        || !writeRegister(REG_CNF3, cnf3))
    {
        return false;
    }

    (void)writeRegister(REG_CANINTE, INTE_RX0 | INTE_RX1);
    (void)writeRegister(REG_RXB0CTRL, RXB0CTRL_VALUE);
    (void)writeRegister(REG_RXB1CTRL, RXB1CTRL_VALUE);
    (void)modifyRegister(REG_CANINTF, 0xFFU, 0x00U);

    if (!setMode(MODE_NORMAL))
    {
        logger::Logger::error(logger::CANSTACK, "MCP2515: normal mode not reached");
        return false;
    }

    m_channelId   = channelId;
    m_baudrate    = baudrate;
    m_initialized = true;

    logger::Logger::debug(
        logger::CANSTACK, "MCP2515 up on channel %u with %lu baud", channelId, baudrate);

    return true;
}

void CanHwMcp2515::shutdown()
{
    (void)setMode(MODE_CONFIG);
    m_initialized = false;
    m_baudrate    = 0U;
}

bool CanHwMcp2515::transmit(
    uint8_t const mailboxId, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    if (!m_initialized)
    {
        return false;
    }

    uint8_t const buffer = (mailboxId == 0U) ? 0U : 1U;
    uint8_t const ctrlReg = (buffer == 0U) ? REG_TXB0CTRL : REG_TXB1CTRL;
    uint8_t const loadInstr = INSTR_LOAD_TX_BUFFER + (buffer * 2U);

    uint8_t ctrl = 0U;
    if (readRegister(ctrlReg, ctrl) && ((ctrl & TXREQ_MASK) != 0U))
    {
        logger::Logger::debug(logger::CANSTACK, "MCP2515: tx buffer %u busy", buffer);
        return false;
    }

    // Load the buffer registers: SIDH, SIDL, EID8, EID0, DLC, data.
    uint8_t bufferBytes[FRAME_REG_COUNT];
    bool const isExtended = (frameId > CanFrame::MAX_BASE_ID);

    if (isExtended)
    {
        bufferBytes[0] = static_cast<uint8_t>(frameId >> 21);
        bufferBytes[1] = static_cast<uint8_t>(((frameId >> 16) & 0x03U) << 6) | SIDL_EXIDE;
        bufferBytes[2] = static_cast<uint8_t>((frameId >> 8) & 0xFFU);
        bufferBytes[3] = static_cast<uint8_t>(frameId & 0xFFU);
    }
    else
    {
        bufferBytes[0] = static_cast<uint8_t>(frameId >> 3);
        bufferBytes[1] = static_cast<uint8_t>((frameId & 0x07U) << SIDL_SID2_0_SHIFT);
        bufferBytes[2] = 0U;
        bufferBytes[3] = 0U;
    }
    bufferBytes[ID_REGISTER_COUNT] = static_cast<uint8_t>(dlc & DLC_MASK);
    for (uint8_t i = 0U; i < DATA_PER_BUFFER; ++i)
    {
        uint8_t const byte = (i < dlc) ? data[i] : 0U;
        bufferBytes[ID_REGISTER_COUNT + 1U + i] = byte;
    }

    uint8_t const txBytes[FRAME_REG_COUNT + 1U] = {
        loadInstr,
        bufferBytes[0],
        bufferBytes[1],
        bufferBytes[2],
        bufferBytes[3],
        bufferBytes[4],
        bufferBytes[5],
        bufferBytes[6],
        bufferBytes[7],
        bufferBytes[8],
        bufferBytes[9],
        bufferBytes[10],
        bufferBytes[11],
        bufferBytes[12],
    };

    if (!m_bus.transfer(txBytes, sizeof(txBytes), nullptr, 0U))
    {
        return false;
    }

    uint8_t const rtsInstr = static_cast<uint8_t>(INSTR_RTS | (1U << buffer));
    return m_bus.transfer(&rtsInstr, 1U, nullptr, 0U);
}

void CanHwMcp2515::registerRxCallback(uint8_t const mailboxId, RxCallback cb)
{
    uint8_t const buffer = (mailboxId < RX_BUFFER_COUNT) ? mailboxId : (RX_BUFFER_COUNT - 1U);
    m_rxCallbacks[buffer] = std::move(cb);
}

void CanHwMcp2515::mainFunction()
{
    if (!m_initialized)
    {
        return;
    }

    uint8_t intFlags = 0U;
    if (!readRegister(REG_CANINTF, intFlags))
    {
        return;
    }

    if ((intFlags & INTF_RX0IF) != 0U)
    {
        dispatchRxBuffer(0U);
        (void)modifyRegister(REG_CANINTF, INTF_RX0IF, 0x00U);
    }

    if ((intFlags & INTF_RX1IF) != 0U)
    {
        dispatchRxBuffer(1U);
        (void)modifyRegister(REG_CANINTF, INTF_RX1IF, 0x00U);
    }

    if ((intFlags & (INTF_ERRIF | INTF_MERRF)) != 0U)
    {
        logger::Logger::debug(logger::CANSTACK, "MCP2515: error interrupt flags 0x%02x", intFlags);
        (void)modifyRegister(REG_CANINTF, (INTF_ERRIF | INTF_MERRF), 0x00U);
    }
}

bool CanHwMcp2515::isInitialized() const { return m_initialized; }

uint32_t CanHwMcp2515::getBaudrate() const { return m_baudrate; }

bool CanHwMcp2515::writeRegister(uint8_t const reg, uint8_t const value)
{
    uint8_t const txBytes[3] = {INSTR_WRITE, reg, value};
    return m_bus.transfer(txBytes, sizeof(txBytes), nullptr, 0U);
}

bool CanHwMcp2515::readRegister(uint8_t const reg, uint8_t& value)
{
    uint8_t const txBytes[2] = {INSTR_READ, reg};
    return m_bus.transfer(txBytes, sizeof(txBytes), &value, 1U);
}

bool CanHwMcp2515::modifyRegister(uint8_t const reg, uint8_t const mask, uint8_t const value)
{
    uint8_t const txBytes[4] = {INSTR_BIT_MODIFY, reg, mask, value};
    return m_bus.transfer(txBytes, sizeof(txBytes), nullptr, 0U);
}

bool CanHwMcp2515::setMode(uint8_t const mode)
{
    if (!modifyRegister(REG_CANCTRL, MODE_MASK, mode))
    {
        return false;
    }

    for (uint8_t i = 0U; i < MODE_POLL_TRIES; ++i)
    {
        uint8_t status = 0U;
        if (readRegister(REG_CANSTAT, status) && ((status & MODE_MASK) == mode))
        {
            return true;
        }
    }
    return false;
}

bool CanHwMcp2515::computeBaudConfig(
    uint32_t const baudrate, uint8_t& cnf1, uint8_t& cnf2, uint8_t& cnf3) const
{
    if ((baudrate == 0U) || (m_oscillatorHz == 0U))
    {
        return false;
    }

    static uint8_t const TQ_PER_BIT_CANDIDATES[] = {16U, 12U, 10U, 9U, 8U, 7U, 6U, 5U};
    static uint8_t const CANDIDATE_COUNT = sizeof(TQ_PER_BIT_CANDIDATES) / sizeof(TQ_PER_BIT_CANDIDATES[0]);

    for (uint8_t i = 0U; i < CANDIDATE_COUNT; ++i)
    {
        uint8_t const tqPerBit = TQ_PER_BIT_CANDIDATES[i];
        uint32_t const divisor = 2U * baudrate * tqPerBit;

        if ((divisor > m_oscillatorHz) || ((m_oscillatorHz % divisor) != 0U))
        {
            continue;
        }

        uint32_t const brp = (m_oscillatorHz / divisor) - 1U;
        if (brp > 0x3FU)
        {
            continue;
        }

        // Bit timing: 1 (sync) + 1 (prop) + PS1 + PS2 with PS1 >= PS2 >= 2.
        uint8_t ps1 = static_cast<uint8_t>((tqPerBit - 1U) / 2U);
        uint8_t ps2 = static_cast<uint8_t>(tqPerBit - 2U - ps1);

        if (ps2 < 2U)
        {
            if (ps1 <= 1U)
            {
                continue;
            }
            ps2 = 2U;
            ps1 = static_cast<uint8_t>(tqPerBit - 4U);
        }

        cnf1 = static_cast<uint8_t>(brp);              // SJW = 1
        cnf2 = static_cast<uint8_t>(0x80U | ((ps1 - 1U) << 3U)); // BTLMODE, PS1, PROP = 1
        cnf3 = static_cast<uint8_t>(ps2 - 1U);

        return true;
    }

    return false;
}

void CanHwMcp2515::dispatchRxBuffer(uint8_t const bufferIndex)
{
    uint8_t const rxBase = ((bufferIndex == 0U) ? REG_RXB0CTRL : REG_RXB1CTRL) + SIDH_OFFSET;
    uint8_t rxBytes[FRAME_REG_COUNT] = {};
    uint8_t const txBytes[2] = {INSTR_READ, rxBase};

    if (!m_bus.transfer(txBytes, sizeof(txBytes), rxBytes, sizeof(rxBytes)))
    {
        return;
    }

    uint32_t frameId = 0U;
    if ((rxBytes[1] & SIDL_EXIDE) != 0U)
    {
        frameId = (static_cast<uint32_t>(rxBytes[0]) << 21)
                  | (static_cast<uint32_t>((rxBytes[1] & SIDL_EID17_16_MASK) >> 6U) << 16)
                  | (static_cast<uint32_t>(rxBytes[2]) << 8)
                  | static_cast<uint32_t>(rxBytes[3]);
    }
    else
    {
        frameId = (static_cast<uint32_t>(rxBytes[0]) << 3)
                  | static_cast<uint32_t>(rxBytes[1] >> SIDL_SID2_0_SHIFT);
    }

    uint8_t const dlc = rxBytes[ID_REGISTER_COUNT] & DLC_MASK;

    RxCallback const& cb = m_rxCallbacks[bufferIndex];
    if (cb)
    {
        cb(frameId, dlc, &rxBytes[ID_REGISTER_COUNT + 1U]);
    }
}

} // namespace canstack
