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
 * \ingroup commgateway
 */
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace commgateway
{

/// One tracked transmission awaiting hardware confirmation.
struct TxConfirmationEntry
{
    uint64_t txId            = 0U;
    std::string signalName;
    uint8_t channelId        = 0U;
    uint32_t frameId         = 0U;
    uint32_t txTimeMs        = 0U;
    uint32_t timeoutMs       = 1000U;
    std::function<void(bool success)> confirmationCallback;
    bool isConfirmed         = false;
};

/**
 * Transmission acknowledgment tracker shared by all protocol channels.
 *
 * Channels register every transmission and report the hardware result via
 * notifyConfirmation(); unconfirmed entries time out in mainFunction() with
 * success = false. Process wide singleton so ISR paths need no wiring.
 */
class TxConfirmationMgr
{
public:
    static TxConfirmationMgr& getInstance();

    void init();
    void shutdown();
    void clear();

    uint64_t registerTx(std::string const& signalName, uint8_t channelId,
                        std::function<void(bool)> callback, uint32_t timeoutMs = 1000U);
    uint64_t registerTx(std::string const& signalName, uint8_t channelId, uint32_t frameId,
                        std::function<void(bool)> callback, uint32_t timeoutMs = 1000U);
    void notifyConfirmation(uint64_t txId, bool success);

    /// Fails expired entries with success = false; call every millisecond.
    void mainFunction(uint32_t nowMs);

    size_t getPendingCount() const;
    uint32_t getTimeoutCount() const;

private:
    TxConfirmationMgr() = default;

    std::map<uint64_t, TxConfirmationEntry> m_pendingTxs;
    uint64_t m_nextTxId     = 1U;
    uint32_t m_timeoutCount = 0U;
    uint32_t m_currentTimeMs = 0U;
    bool m_initialized      = false;
};

} // namespace commgateway
