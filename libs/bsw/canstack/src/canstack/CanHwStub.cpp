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
#include "canstack/CanHwStub.h"
#include "canstack/CanStackLogger.h"

#include <cstring>

namespace canstack
{
namespace logger = ::util::logger;

void CanHwStub::setLoopback(bool const enabled) { m_loopback = enabled; }

bool CanHwStub::init(uint8_t const channelId, uint32_t const baudrate)
{
    m_channelId  = channelId;
    m_baudrate   = baudrate;
    m_initialized = true;

    logger::Logger::debug(logger::CANSTACK, "CanHwStub %u initialized with %lu baud", channelId, baudrate);

    return true;
}

void CanHwStub::shutdown()
{
    m_rxQueue.clear();
    m_initialized = false;
}

bool CanHwStub::transmit(
    uint8_t const /*mailboxId*/, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    if (!m_initialized)
    {
        return false;
    }

    CanFrame frame(frameId, dlc, data);
    m_txLog.push_back(frame);

    if (m_loopback)
    {
        RxItem item;
        item.frameId = frameId;
        item.dlc    = dlc;
        (void)memcpy(item.data, frame.getData(), static_cast<size_t>(dlc));
        m_rxQueue.push_back(item);
    }

    return true;
}

void CanHwStub::registerRxCallback(uint8_t const mailboxId, RxCallback cb)
{
    for (auto& entry : m_rxCallbacks)
    {
        if (entry.first == mailboxId)
        {
            entry.second = std::move(cb);
            return;
        }
    }

    auto insertPos = m_rxCallbacks.begin();
    while ((insertPos != m_rxCallbacks.end()) && (insertPos->first < mailboxId))
    {
        ++insertPos;
    }
    m_rxCallbacks.insert(insertPos, std::make_pair(mailboxId, std::move(cb)));
}

void CanHwStub::mainFunction()
{
    while (!m_rxQueue.empty())
    {
        RxItem const item = m_rxQueue.front();
        m_rxQueue.pop_front();
        dispatch(item);
    }
}

void CanHwStub::injectRxFrame(uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    RxItem item;
    item.frameId = frameId;
    item.dlc     = dlc;
    if ((data != nullptr) && (dlc > 0U))
    {
        (void)memcpy(item.data, data, static_cast<size_t>(dlc));
    }
    m_rxQueue.push_back(item);
}

std::vector<CanFrame> const& CanHwStub::getTxLog() const { return m_txLog; }

void CanHwStub::clearTxLog() { m_txLog.clear(); }

bool CanHwStub::isInitialized() const { return m_initialized; }

uint8_t CanHwStub::getChannelId() const { return m_channelId; }

uint32_t CanHwStub::getBaudrate() const { return m_baudrate; }

void CanHwStub::dispatch(RxItem const& item)
{
    // Single delivery like a real controller: the lowest mailbox accepting
    // the frame receives it.
    for (auto const& entry : m_rxCallbacks)
    {
        if (entry.second)
        {
            entry.second(item.frameId, item.dlc, item.data);
            return;
        }
    }
}

} // namespace canstack
