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
#include "commgateway/TxConfirmationMgr.h"

namespace commgateway
{

TxConfirmationMgr& TxConfirmationMgr::getInstance()
{
    static TxConfirmationMgr instance;
    return instance;
}

void TxConfirmationMgr::init() { m_initialized = true; }

void TxConfirmationMgr::shutdown()
{
    m_initialized = false;
    clear();
}

void TxConfirmationMgr::clear()
{
    m_pendingTxs.clear();
    m_timeoutCount = 0U;
}

uint64_t TxConfirmationMgr::registerTx(std::string const& signalName, uint8_t channelId,
                                       std::function<void(bool)> callback, uint32_t timeoutMs)
{
    return registerTx(signalName, channelId, 0U, callback, timeoutMs);
}

uint64_t TxConfirmationMgr::registerTx(std::string const& signalName, uint8_t channelId,
                                       uint32_t frameId, std::function<void(bool)> callback,
                                       uint32_t timeoutMs)
{
    TxConfirmationEntry entry{};
    entry.txId                  = m_nextTxId++;
    entry.signalName            = signalName;
    entry.channelId             = channelId;
    entry.frameId               = frameId;
    entry.txTimeMs              = m_currentTimeMs;
    entry.timeoutMs             = timeoutMs;
    entry.confirmationCallback  = callback;
    m_pendingTxs[entry.txId] = entry;
    return entry.txId;
}

void TxConfirmationMgr::notifyConfirmation(uint64_t txId, bool success)
{
    auto it = m_pendingTxs.find(txId);
    if (it == m_pendingTxs.end())
    {
        return;
    }
    TxConfirmationEntry entry = it->second;
    m_pendingTxs.erase(it);
    entry.isConfirmed = true;
    if (entry.confirmationCallback)
    {
        entry.confirmationCallback(success);
    }
}

void TxConfirmationMgr::mainFunction(uint32_t nowMs)
{
    if (!m_initialized)
    {
        return;
    }
    m_currentTimeMs = nowMs;
    for (auto it = m_pendingTxs.begin(); it != m_pendingTxs.end();)
    {
        if (((nowMs - it->second.txTimeMs) >= it->second.timeoutMs)
            && (it->second.timeoutMs > 0U))
        {
            TxConfirmationEntry entry = it->second;
            it                        = m_pendingTxs.erase(it);
            m_timeoutCount++;
            if (entry.confirmationCallback)
            {
                entry.confirmationCallback(false);
            }
        }
        else
        {
            ++it;
        }
    }
}

size_t TxConfirmationMgr::getPendingCount() const { return m_pendingTxs.size(); }

uint32_t TxConfirmationMgr::getTimeoutCount() const { return m_timeoutCount; }

} // namespace commgateway
