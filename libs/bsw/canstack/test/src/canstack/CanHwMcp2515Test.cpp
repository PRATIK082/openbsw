/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "canstack/CanHwMcp2515.h"

#include <gmock/gmock.h>

#include <cstdint>
#include <vector>

namespace
{
using namespace ::testing;

/**
 * Register level fake of the MCP2515 SPI bus.
 *
 * Implements just enough of the chip (mode registers, config registers,
 * transmit buffers, receive buffers, interrupt flags) to exercise the driver
 * without hardware. CANSTAT follows the CANCTRL mode bits like the real chip
 * does once the oscillator has stabilized.
 */
class FakeMcp2515Bus : public ::canstack::IMcp2515Bus
{
public:
    // Instruction bytes relevant for the tests.
    static uint8_t const INSTR_WRITE      = 0x02U;
    static uint8_t const INSTR_READ       = 0x03U;
    static uint8_t const INSTR_BIT_MODIFY = 0x05U;
    static uint8_t const INSTR_LOAD_TX0   = 0x40U;
    static uint8_t const INSTR_LOAD_TX1   = 0x42U;
    static uint8_t const INSTR_RTS        = 0x80U;
    static uint8_t const INSTR_RESET      = 0xC0U;
    static uint8_t const REG_CANCTRL      = 0x0FU;
    static uint8_t const REG_CANSTAT      = 0x1EU;
    static uint8_t const REG_CANINTF      = 0x2CU;
    static uint8_t const REG_TXB0_SIDH    = 0x31U;
    static uint8_t const REG_TXB1_SIDH    = 0x41U;
    static uint8_t const REG_RXB0_SIDH    = 0x61U;

    bool transfer(uint8_t const* txData, uint8_t txLen, uint8_t* rxData, uint8_t rxLen) override
    {
        if (txLen == 0U)
        {
            return false;
        }

        uint8_t const instruction = txData[0];

        switch (instruction)
        {
            case INSTR_RESET:
                registers[REG_CANCTRL] = 0x80U; // reset enters configuration mode
                registers[REG_CANSTAT] = 0x80U;
                return true;

            case INSTR_WRITE:
                if (txLen < 3U)
                {
                    return false;
                }
                registers[txData[1]] = txData[2];
                if (txData[1] == REG_CANCTRL)
                {
                    registers[REG_CANSTAT] = static_cast<uint8_t>(txData[2] & 0xE0U);
                }
                return true;

            case INSTR_READ:
                if ((txLen >= 2U) && (rxData != nullptr) && (rxLen > 0U))
                {
                    for (uint8_t i = 0U; i < rxLen; ++i)
                    {
                        rxData[i] = registers[txData[1] + i];
                    }
                }
                return true;

            case INSTR_BIT_MODIFY:
                if (txLen < 4U)
                {
                    return false;
                }
                registers[txData[1]] = static_cast<uint8_t>(
                    (registers[txData[1]] & static_cast<uint8_t>(~txData[2]))
                    | static_cast<uint8_t>(txData[3] & txData[2]));
                if (txData[1] == REG_CANCTRL)
                {
                    registers[REG_CANSTAT] = static_cast<uint8_t>(registers[REG_CANCTRL] & 0xE0U);
                }
                return true;

            case INSTR_LOAD_TX0:
            case INSTR_LOAD_TX1:
            {
                uint8_t const base = (instruction == INSTR_LOAD_TX0) ? REG_TXB0_SIDH : REG_TXB1_SIDH;
                std::vector<uint8_t> payload(txData + 1U, txData + txLen);
                for (uint8_t i = 0U; i < payload.size(); ++i)
                {
                    registers[base + i] = payload[i];
                }
                txLoads.push_back(payload);
                return true;
            }

            default:
                if ((instruction & 0xF8U) == INSTR_RTS)
                {
                    rtsMask.push_back(static_cast<uint8_t>(instruction & 0x07U));
                    return true;
                }
                return false;
        }
    }

    /// Loads receive buffer 0 registers: SIDH, SIDL, EID8, EID0, DLC, data.
    void loadRxBuffer0(std::vector<uint8_t> const& frameRegisters)
    {
        for (uint8_t i = 0U; i < frameRegisters.size(); ++i)
        {
            registers[REG_RXB0_SIDH + i] = frameRegisters[i];
        }
        registers[REG_CANINTF] |= 0x01U; // RX0IF
    }

    uint8_t registers[256] = {};
    std::vector<uint8_t> rtsMask;
    std::vector<std::vector<uint8_t>> txLoads;
};

/**
 * \desc: init() brings the chip up with the documented bit timing for
 * 500 kbit/s at 8 MHz (CNF1=0x00, CNF2=0x90, CNF3=0x02) and switches to
 * normal mode.
 */
TEST(CanHwMcp2515Test, init_writes_baud_config_and_normal_mode)
{
    FakeMcp2515Bus bus;
    ::canstack::CanHwMcp2515 driver(bus);

    EXPECT_TRUE(driver.init(0U, 500000U));
    EXPECT_TRUE(driver.isInitialized());
    EXPECT_EQ(500000U, driver.getBaudrate());

    EXPECT_EQ(0x00U, bus.registers[0x2AU]); // CNF1
    EXPECT_EQ(0x90U, bus.registers[0x29U]); // CNF2
    EXPECT_EQ(0x02U, bus.registers[0x28U]); // CNF3
    EXPECT_EQ(0x00U, bus.registers[0x0FU] & 0xE0U); // normal mode
}

/**
 * \desc: Baudrates unreachable with the 8 MHz crystal are rejected.
 */
TEST(CanHwMcp2515Test, unreachable_baudrate_fails_init)
{
    FakeMcp2515Bus bus;
    ::canstack::CanHwMcp2515 driver(bus);

    EXPECT_FALSE(driver.init(0U, 123456U));
    EXPECT_FALSE(driver.isInitialized());
}

/**
 * \desc: transmit() loads the standard identifier registers of buffer 0 and
 * raises the RTS flag.
 */
TEST(CanHwMcp2515Test, transmit_standard_frame)
{
    FakeMcp2515Bus bus;
    ::canstack::CanHwMcp2515 driver(bus);
    (void)driver.init(0U, 500000U);

    uint8_t const data[4] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
    EXPECT_TRUE(driver.transmit(0U, 0x123U, 4U, data));

    ASSERT_EQ(1U, bus.txLoads.size());
    // SIDH, SIDL, EID8, EID0, DLC, data...
    EXPECT_EQ(0x24U, bus.txLoads[0][0]); // 0x123 >> 3
    EXPECT_EQ(0x60U, bus.txLoads[0][1]); // (0x123 & 7) << 5
    EXPECT_EQ(4U, bus.txLoads[0][4]);    // DLC
    EXPECT_EQ(0xDEU, bus.txLoads[0][5]);
    EXPECT_EQ(0xEFU, bus.txLoads[0][8]);

    ASSERT_EQ(1U, bus.rtsMask.size());
    EXPECT_EQ(0x01U, bus.rtsMask[0]); // RTS TXB0
}

/**
 * \desc: transmit() encodes extended identifiers into SIDL/EID8/EID0.
 */
TEST(CanHwMcp2515Test, transmit_extended_frame)
{
    FakeMcp2515Bus bus;
    ::canstack::CanHwMcp2515 driver(bus);
    (void)driver.init(0U, 500000U);

    uint8_t const data[1] = {0x42U};
    uint32_t const extendedId = 0x18ABCDEFU;
    EXPECT_TRUE(driver.transmit(0U, extendedId, 1U, data));

    ASSERT_EQ(1U, bus.txLoads.size());
    EXPECT_EQ(0xC5U, bus.txLoads[0][0]); // id >> 21
    EXPECT_EQ(0xC8U, bus.txLoads[0][1]); // ((id >> 16) & 3) << 6 | EXIDE
    EXPECT_EQ(0xCDU, bus.txLoads[0][2]); // (id >> 8) & 0xFF
    EXPECT_EQ(0xEFU, bus.txLoads[0][3]); // id & 0xFF
    EXPECT_EQ(1U, bus.txLoads[0][4]);    // DLC
}

/**
 * \desc: A frame received into buffer 0 is parsed (standard id) and dispatched
 * to the callback registered for mailbox 0 from mainFunction().
 */
TEST(CanHwMcp2515Test, receive_standard_frame)
{
    FakeMcp2515Bus bus;
    ::canstack::CanHwMcp2515 driver(bus);
    (void)driver.init(0U, 500000U);

    uint32_t receivedId = 0U;
    uint8_t receivedDlc = 0U;
    uint8_t receivedData[8] = {};
    driver.registerRxCallback(
        0U, [&receivedId, &receivedDlc, &receivedData](uint32_t id, uint8_t dlc, uint8_t const* data) {
            receivedId   = id;
            receivedDlc = dlc;
            for (uint8_t i = 0U; i < dlc; ++i)
            {
                receivedData[i] = data[i];
            }
        });

    // Standard id 0x201, dlc 2, payload {0xCA, 0xFE}.
    bus.loadRxBuffer0({0x40U, 0x20U, 0x00U, 0x00U, 0x02U, 0xCAU, 0xFEU});

    driver.mainFunction();

    EXPECT_EQ(0x201U, receivedId);
    EXPECT_EQ(2U, receivedDlc);
    EXPECT_EQ(0xCAU, receivedData[0]);
    EXPECT_EQ(0xFEU, receivedData[1]);
    EXPECT_EQ(0x00U, bus.registers[0x2CU] & 0x01U); // RX0IF cleared
}

/**
 * \desc: An extended frame in buffer 0 is reassembled from the id registers.
 */
TEST(CanHwMcp2515Test, receive_extended_frame)
{
    FakeMcp2515Bus bus;
    ::canstack::CanHwMcp2515 driver(bus);
    (void)driver.init(0U, 500000U);

    uint32_t receivedId = 0U;
    driver.registerRxCallback(0U, [&receivedId](uint32_t id, uint8_t, uint8_t const*) {
        receivedId = id;
    });

    uint32_t const extendedId = 0x1F234567U;
    bus.loadRxBuffer0({static_cast<uint8_t>(extendedId >> 21),
                      static_cast<uint8_t>((((extendedId >> 16) & 0x03U) << 6) | 0x08U),
                      static_cast<uint8_t>((extendedId >> 8) & 0xFFU),
                      static_cast<uint8_t>(extendedId & 0xFFU),
                      1U,
                      0x99U});

    driver.mainFunction();

    EXPECT_EQ(extendedId, receivedId);
}

} // namespace
