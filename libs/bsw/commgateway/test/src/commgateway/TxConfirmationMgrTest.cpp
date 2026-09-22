/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "commgateway/TxConfirmationMgr.h"

#include <gmock/gmock.h>

namespace
{
using namespace ::testing;

/**
 * \desc: Confirmed transmissions invoke the callback with success.
 */
TEST(TxConfirmationMgrTest, confirm_success)
{
    auto& mgr = ::commgateway::TxConfirmationMgr::getInstance();
    mgr.init();
    mgr.clear();

    bool called   = false;
    bool reported = false;
    uint64_t const txId
        = mgr.registerTx("Speed", 0U, [&called, &reported](bool success) {
              called   = true;
              reported = success;
          });
    EXPECT_EQ(1U, mgr.getPendingCount());

    mgr.notifyConfirmation(txId, true);
    EXPECT_TRUE(called);
    EXPECT_TRUE(reported);
    EXPECT_EQ(0U, mgr.getPendingCount());

    mgr.notifyConfirmation(txId, true); // duplicate: no-op, no crash
    mgr.notifyConfirmation(0xDEADU, true); // unknown id: no-op
}

/**
 * \desc: Unconfirmed transmissions time out with success = false.
 */
TEST(TxConfirmationMgrTest, timeout_fails)
{
    auto& mgr = ::commgateway::TxConfirmationMgr::getInstance();
    mgr.init();
    mgr.clear();

    bool called   = false;
    bool reported = true;
    mgr.mainFunction(0U);
    (void)mgr.registerTx("Speed", 0U, [&called, &reported](bool success) {
        called   = true;
        reported = success;
    }, 100U);

    mgr.mainFunction(99U);
    EXPECT_FALSE(called);
    mgr.mainFunction(100U);
    EXPECT_TRUE(called);
    EXPECT_FALSE(reported);
    EXPECT_EQ(1U, mgr.getTimeoutCount());
    EXPECT_EQ(0U, mgr.getPendingCount());
}

} // namespace
