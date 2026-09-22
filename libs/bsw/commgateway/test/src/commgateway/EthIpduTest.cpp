/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/EthChannel.h"
#include "commgateway/EthIpdu.h"

#include <gmock/gmock.h>

#include <vector>

namespace
{
using namespace ::testing;

/**
 * \desc: transmitPdu sends the datagram and loopback redelivers it as the
 * primary PDU.
 */
TEST(EthIpduTest, transmit_and_loopback_rx)
{
    ::commgateway::EthLoopbackChannel transport;
    ::commgateway::EthIpduManager manager;

    ::commgateway::EthIpduConfig config{};
    config.destIp       = 0xC0A80001U;
    config.destPort     = 3000U;
    config.srcPort      = 3001U;
    config.primaryPduId = 4096U;
    manager.init(config, &transport);
    ASSERT_TRUE(manager.isInitialized());

    std::vector<std::tuple<uint32_t, uint16_t>> received;
    manager.registerPduCallback(
        [&received](uint32_t pduId, uint16_t length, uint8_t const*) {
            received.emplace_back(pduId, length);
        });

    uint8_t const data[4U] = {1U, 2U, 3U, 4U};
    EXPECT_TRUE(manager.transmitPdu(4096U, 4U, data));
    ASSERT_EQ(1U, transport.getTxLog().size());
    EXPECT_EQ(3000U, transport.getTxLog()[0].destPort);

    ASSERT_EQ(1U, received.size());
    EXPECT_EQ(4096U, std::get<0>(received[0]));
    EXPECT_EQ(4U, std::get<1>(received[0]));
    EXPECT_EQ(1U, manager.getTxCount());
    EXPECT_EQ(1U, manager.getRxCount());
}

/**
 * \desc: Registered source-port mappings demultiplex to the right PDU.
 */
TEST(EthIpduTest, port_mapping)
{
    ::commgateway::EthLoopbackChannel transport;
    transport.setLoopback(false);
    ::commgateway::EthIpduManager manager;

    ::commgateway::EthIpduConfig config{};
    config.primaryPduId = 100U;
    manager.init(config, &transport);
    manager.registerPduMapping(5000U, 200U);

    std::vector<uint32_t> received;
    manager.registerPduCallback([&received](uint32_t pduId, uint16_t, uint8_t const*) {
        received.push_back(pduId);
    });

    uint8_t const data[1U] = {0U};
    transport.injectRx(0U, 5000U, data, 1U);
    transport.injectRx(0U, 6000U, data, 1U);
    ASSERT_EQ(2U, received.size());
    EXPECT_EQ(200U, received[0]);
    EXPECT_EQ(100U, received[1]);
}

/**
 * \desc: With cycleTimeMs set, mainFunction() retransmits the last frame.
 */
TEST(EthIpduTest, cyclic_retransmission)
{
    ::commgateway::EthLoopbackChannel transport;
    transport.setLoopback(false);
    ::commgateway::EthIpduManager manager;

    ::commgateway::EthIpduConfig config{};
    config.cycleTimeMs  = 100U;
    config.primaryPduId = 7U;
    manager.init(config, &transport);

    uint8_t const data[2U] = {0xDEU, 0xADU};
    EXPECT_TRUE(manager.transmitPdu(7U, 2U, data));
    EXPECT_EQ(1U, transport.getTxLog().size());

    manager.mainFunction(50U);
    EXPECT_EQ(1U, transport.getTxLog().size());
    manager.mainFunction(100U);
    EXPECT_EQ(2U, transport.getTxLog().size());
}

} // namespace
