/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/DoIpHandler.h"

#include <gmock/gmock.h>

#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

std::vector<uint8_t> makeDoIp(uint16_t payloadType, std::vector<uint8_t> const& payload)
{
    std::vector<uint8_t> frame(8U + payload.size(), 0U);
    frame[0] = 0x02U;
    frame[1] = 0xFDU;
    frame[2] = static_cast<uint8_t>((payloadType >> 8U) & 0xFFU);
    frame[3] = static_cast<uint8_t>(payloadType & 0xFFU);
    frame[4] = 0U;
    frame[5] = 0U;
    frame[6] = static_cast<uint8_t>((payload.size() >> 8U) & 0xFFU);
    frame[7] = static_cast<uint8_t>(payload.size() & 0xFFU);
    for (size_t i = 0U; i < payload.size(); ++i)
    {
        frame[8U + i] = payload[i];
    }
    return frame;
}

/**
 * \desc: Routing activation enables the channel and answers with success.
 */
TEST(DoIpHandlerTest, routing_activation)
{
    ::framegateway::DoIpHandler handler;
    ::framegateway::DoIpConfig config{};
    handler.init(config);

    std::vector<std::vector<uint8_t>> sent;
    handler.setTxSender([&sent](uint16_t len, uint8_t const* data) {
        sent.emplace_back(data, data + len);
        return true;
    });

    EXPECT_FALSE(handler.isRoutingActive());
    std::vector<uint8_t> payload = {0x0EU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    std::vector<uint8_t> frame
        = makeDoIp(::framegateway::DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ, payload);
    handler.onDoIpMessageReceived(frame.data(), static_cast<uint16_t>(frame.size()));
    EXPECT_TRUE(handler.isRoutingActive());
    ASSERT_EQ(1U, sent.size());
    EXPECT_EQ(0x0006U, (static_cast<uint16_t>(sent[0][2]) << 8U) | sent[0][3]);
    EXPECT_EQ(1U, handler.getRxCount());
    EXPECT_EQ(1U, handler.getTxCount());
}

/**
 * \desc: UDS messages pass SA/TA/payload to the callback once active.
 */
TEST(DoIpHandlerTest, uds_message_routing)
{
    ::framegateway::DoIpHandler handler;
    handler.init(::framegateway::DoIpConfig{});

    std::vector<std::tuple<uint16_t, uint16_t, std::vector<uint8_t>>> uds;
    handler.setUdsCallback(
        [&uds](uint16_t sa, uint16_t ta, uint16_t len, uint8_t const* data) {
            uds.emplace_back(sa, ta, std::vector<uint8_t>(data, data + len));
        });
    handler.setTxSender([](uint16_t, uint8_t const*) { return true; });

    // Not active yet: ignored.
    std::vector<uint8_t> payload = {0x0EU, 0x00U, 0x10U, 0x00U, 0x22U};
    std::vector<uint8_t> frame = makeDoIp(::framegateway::DOIP_PAYLOAD_UDS_MESSAGE, payload);
    handler.onDoIpMessageReceived(frame.data(), static_cast<uint16_t>(frame.size()));
    EXPECT_TRUE(uds.empty());

    std::vector<uint8_t> actPayload = {0x0EU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    std::vector<uint8_t> act
        = makeDoIp(::framegateway::DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ, actPayload);
    handler.onDoIpMessageReceived(act.data(), static_cast<uint16_t>(act.size()));
    handler.onDoIpMessageReceived(frame.data(), static_cast<uint16_t>(frame.size()));
    ASSERT_EQ(1U, uds.size());
    EXPECT_EQ(0x0E00U, std::get<0>(uds[0]));
    EXPECT_EQ(0x1000U, std::get<1>(uds[0]));
    EXPECT_THAT(std::get<2>(uds[0]), ElementsAre(0x22U));
}

/**
 * \desc: Malformed headers and version mismatches are dropped silently.
 */
TEST(DoIpHandlerTest, rejects_malformed)
{
    ::framegateway::DoIpHandler handler;
    handler.init(::framegateway::DoIpConfig{});
    handler.setTxSender([](uint16_t, uint8_t const*) { return true; });

    uint8_t shortFrame[4U] = {0x02U, 0xFDU, 0x00U, 0x05U};
    handler.onDoIpMessageReceived(shortFrame, 4U);

    std::vector<uint8_t> badVer = {0x01U, 0xFEU, 0x00U, 0x05U, 0U, 0U, 0U, 7U,
                                   0U, 0U, 0U, 0U, 0U, 0U, 0U};
    handler.onDoIpMessageReceived(badVer.data(), static_cast<uint16_t>(badVer.size()));

    // Declared length longer than the datagram.
    std::vector<uint8_t> truncated = {0x02U, 0xFDU, 0x00U, 0x05U, 0U, 0U, 0U, 7U, 0x0EU};
    handler.onDoIpMessageReceived(truncated.data(), static_cast<uint16_t>(truncated.size()));

    EXPECT_EQ(0U, handler.getRxCount());
    EXPECT_FALSE(handler.isRoutingActive());
}

/**
 * \desc: sendUdsMessage builds a well-formed DoIP UDS datagram.
 */
TEST(DoIpHandlerTest, send_uds_message)
{
    ::framegateway::DoIpHandler handler;
    handler.init(::framegateway::DoIpConfig{});

    std::vector<uint8_t> sent;
    handler.setTxSender([&sent](uint16_t len, uint8_t const* data) {
        sent.assign(data, data + len);
        return true;
    });

    uint8_t const uds[2U] = {0x3EU, 0x00U};
    ASSERT_TRUE(handler.sendUdsMessage(0x1000U, 0x0E00U, uds, 2U));
    ASSERT_EQ(14U, sent.size());
    EXPECT_EQ(0x02U, sent[0]);
    EXPECT_EQ(0x80U, sent[2]);
    EXPECT_EQ(0x01U, sent[3]);
    EXPECT_EQ(0x10U, sent[8]);
    EXPECT_EQ(0x00U, sent[9]);
    EXPECT_EQ(0x0EU, sent[10]);
    EXPECT_EQ(0x3EU, sent[12]);
}

} // namespace
