/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "framegateway/DiagLink.h"

#include <gmock/gmock.h>

#include <tuple>
#include <vector>

namespace
{
using namespace ::testing;

::framegateway::DiagSession makeSession()
{
    ::framegateway::DiagSession session{};
    session.channelType      = 0U;
    session.requestFrameId   = 0x7E0U;
    session.responseFrameId  = 0x7E8U;
    session.sessionTimeoutMs = 5000U;
    return session;
}

/**
 * \desc: Requests reach the UDS handler; responses transmit on the response id.
 */
TEST(DiagLinkTest, request_response)
{
    ::framegateway::DiagLink link;
    link.init({makeSession()});

    std::vector<std::tuple<uint8_t, uint8_t, uint32_t, uint16_t>> sent;
    link.setTxSender(
        [&sent](uint8_t t, uint8_t c, uint32_t id, uint16_t len, uint8_t const*) {
            sent.emplace_back(t, c, id, len);
            return true;
        });
    link.setUdsHandler([](uint16_t reqLen, uint8_t const* req, uint8_t* resp, uint16_t& respLen) {
        respLen = (reqLen < respLen) ? reqLen : respLen;
        for (uint16_t i = 0U; i < respLen; ++i)
        {
            resp[i] = static_cast<uint8_t>(req[i] + 0x40U); // positive response mock
        }
    });

    uint8_t const request[3U] = {0x22U, 0xF1U, 0x90U};
    link.onDiagnosticPduReceived(0x7E0U, 3U, request, 0U);
    ASSERT_EQ(1U, sent.size());
    EXPECT_EQ(0x7E8U, std::get<2>(sent[0]));
    EXPECT_EQ(3U, std::get<3>(sent[0]));
    EXPECT_EQ(1U, link.getRequestCount());
    EXPECT_EQ(1U, link.getResponseCount());
}

/**
 * \desc: Unknown PDUs and inactive sessions are ignored.
 */
TEST(DiagLinkTest, ignores_unknown_and_inactive)
{
    ::framegateway::DiagLink link;
    link.init({makeSession()});
    link.setUdsHandler([](uint16_t, uint8_t const*, uint8_t*, uint16_t& respLen) {
        respLen = 0U;
    });

    uint8_t const request[1U] = {0x10U};
    link.onDiagnosticPduReceived(0x7DFU, 1U, request, 0U); // unknown
    link.setSessionActive(0x7E0U, false, 0U);
    link.onDiagnosticPduReceived(0x7E0U, 1U, request, 0U); // inactive
    EXPECT_EQ(0U, link.getRequestCount());
}

/**
 * \desc: Quiet sessions time out and stop answering.
 */
TEST(DiagLinkTest, session_timeout)
{
    ::framegateway::DiagSession session = makeSession();
    session.sessionTimeoutMs            = 100U;
    ::framegateway::DiagLink link;
    link.init({session});
    link.setUdsHandler([](uint16_t, uint8_t const*, uint8_t*, uint16_t& respLen) {
        respLen = 1U;
    });
    link.setTxSender([](uint8_t, uint8_t, uint32_t, uint16_t, uint8_t const*) {
        return true;
    });

    uint8_t const request[1U] = {0x3EU};
    link.onDiagnosticPduReceived(0x7E0U, 1U, request, 0U);
    EXPECT_EQ(1U, link.getRequestCount());
    link.mainFunction(100U);
    EXPECT_EQ(1U, link.getSessionTimeoutCount());
    link.onDiagnosticPduReceived(0x7E0U, 1U, request, 200U);
    EXPECT_EQ(1U, link.getRequestCount()); // timed out: no answer
}

} // namespace
