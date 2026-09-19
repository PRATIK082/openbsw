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

#include "canstack/CanHwInterface.h"

#include <cstdint>

namespace canstack
{
/**
 * SPI transport abstraction for the MCP2515 driver.
 *
 * Implementations drive the chip select, clock and data lines of one MCP2515.
 * A transfer asserts chip select, shifts out the tx bytes and then shifts in
 * rx bytes while chip select stays asserted.
 */
class IMcp2515Bus
{
public:
    virtual ~IMcp2515Bus() = default;

    /**
     * \param txData bytes to shift out (may be nullptr if txLen == 0)
     * \param txLen number of bytes to shift out
     * \param rxData buffer receiving the bytes shifted in after the tx bytes
     * (may be nullptr if rxLen == 0)
     * \param rxLen number of bytes to shift in
     * \return true if the transfer succeeded
     */
    virtual bool
    transfer(uint8_t const* txData, uint8_t txLen, uint8_t* rxData, uint8_t rxLen) = 0;
};

/**
 * Register level driver for the Microchip MCP2515 stand-alone CAN controller.
 *
 * The driver talks to the chip through an IMcp2515Bus (typically SPI) and keeps
 * all chip specific register access inside this class. Reception is polled:
 * registerRxCallback(0, cb) subscribes to receive buffer 0, registerRxCallback(1, cb)
 * to receive buffer 1; frames are dispatched from mainFunction().
 *
 * transmit() uses transmit buffer 0 for mailboxId 0 and buffer 1 otherwise.
 */
class CanHwMcp2515 final : public CanHwInterface
{
public:
    /// MCP2515 crystal frequency the baudrate calculation is based on.
    static uint32_t const DEFAULT_OSCILLATOR_HZ = 8000000U;

    /// Number of supported receive buffers (RXB0 and RXB1).
    static uint8_t const RX_BUFFER_COUNT = 2U;

    /**
     * \param bus SPI transport of this chip
     * \param oscillatorHz frequency of the crystal attached to the chip
     */
    explicit CanHwMcp2515(IMcp2515Bus& bus, uint32_t oscillatorHz = DEFAULT_OSCILLATOR_HZ);

    bool init(uint8_t channelId, uint32_t baudrate) override;
    void shutdown() override;
    bool transmit(uint8_t mailboxId, uint32_t frameId, uint8_t dlc, uint8_t const* data) override;
    void registerRxCallback(uint8_t mailboxId, RxCallback cb) override;
    void mainFunction() override;

    /// \return true while the controller is initialized and in normal mode
    bool isInitialized() const;

    /// \return the last configured baudrate (0 if not initialized)
    uint32_t getBaudrate() const;

private:
    // Instruction set (MCP2515 datasheet, instruction formats).
    static uint8_t const INSTR_WRITE           = 0x02U;
    static uint8_t const INSTR_READ           = 0x03U;
    static uint8_t const INSTR_BIT_MODIFY     = 0x05U;
    static uint8_t const INSTR_LOAD_TX_BUFFER = 0x40U; ///< base, +2 = TX buffer n SIDH
    static uint8_t const INSTR_RTS             = 0x80U; ///< base, bit n = request buffer n
    static uint8_t const INSTR_RESET           = 0xC0U;

    // Register map (base addresses; buffer registers follow consecutively).
    static uint8_t const REG_CANCTRL  = 0x0FU;
    static uint8_t const REG_CANSTAT  = 0x1EU;
    static uint8_t const REG_CNF3     = 0x28U;
    static uint8_t const REG_CNF2     = 0x29U;
    static uint8_t const REG_CNF1     = 0x2AU;
    static uint8_t const REG_CANINTE  = 0x2BU;
    static uint8_t const REG_CANINTF  = 0x2CU;
    static uint8_t const REG_TXB0CTRL = 0x30U; ///< SIDH 0x31 .. data 0x36..0x3D
    static uint8_t const REG_TXB1CTRL = 0x40U; ///< SIDH 0x41 .. data 0x46..0x4D
    static uint8_t const REG_RXB0CTRL = 0x60U; ///< SIDH 0x61 .. data 0x66..0x6D
    static uint8_t const REG_RXB1CTRL = 0x70U; ///< SIDH 0x71 .. data 0x76..0x7D
    static uint8_t const SIDH_OFFSET  = 1U;

    // CANCTRL/CANSTAT mode bits.
    static uint8_t const MODE_NORMAL      = 0x00U;
    static uint8_t const MODE_CONFIG      = 0x80U;
    static uint8_t const MODE_MASK        = 0xE0U;
    static uint8_t const MODE_POLL_TRIES  = 10U;

    // RXBnCTRL: RXM bits 1..0 = 11b (receive any), BUKT bit 2 (rollover RXB0 -> RXB1).
    static uint8_t const RXB0CTRL_VALUE = 0x64U;
    static uint8_t const RXB1CTRL_VALUE = 0x60U;

    // CANINTE/CANINTF bits.
    static uint8_t const INTE_RX0 = 0x01U;
    static uint8_t const INTE_RX1 = 0x02U;
    static uint8_t const INTF_RX0IF = 0x01U;
    static uint8_t const INTF_RX1IF = 0x02U;
    static uint8_t const INTF_ERRIF = 0x04U;
    static uint8_t const INTF_MERRF = 0x80U;

    // TXBnCTRL bit 3: TXREQ (transmission pending).
    static uint8_t const TXREQ_MASK = 0x08U;

    static uint8_t const DLC_MASK    = 0x0FU;
    static uint8_t const SIDL_EXIDE  = 0x08U;
    static uint8_t const SIDL_EID17_16_MASK = 0xC0U;
    static uint8_t const SIDL_SID2_0_SHIFT   = 5U;
    static uint8_t const DATA_PER_BUFFER      = 8U;
    static uint8_t const ID_REGISTER_COUNT    = 4U; ///< SIDH, SIDL, EID8, EID0
    static uint8_t const FRAME_REG_COUNT      = ID_REGISTER_COUNT + 1U + DATA_PER_BUFFER;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);
    bool modifyRegister(uint8_t reg, uint8_t mask, uint8_t value);
    bool setMode(uint8_t mode);
    bool computeBaudConfig(uint32_t baudrate, uint8_t& cnf1, uint8_t& cnf2, uint8_t& cnf3) const;
    void dispatchRxBuffer(uint8_t bufferIndex);

    IMcp2515Bus& m_bus;
    uint32_t const m_oscillatorHz;
    bool m_initialized = false;
    uint8_t m_channelId = 0U;
    uint32_t m_baudrate = 0U;
    RxCallback m_rxCallbacks[RX_BUFFER_COUNT];
};

} // namespace canstack
