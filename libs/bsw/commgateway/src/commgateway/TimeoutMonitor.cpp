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
#include "commgateway/TimeoutMonitor.h"

namespace commgateway
{

void TimeoutMonitor::init() { m_initialized = true; }

void TimeoutMonitor::shutdown()
{
    m_initialized = false;
    clear();
}

void TimeoutMonitor::clear()
{
    m_timeoutTable.clear();
    m_timeoutCount = 0U;
}

void TimeoutMonitor::registerSignalTimeout(std::string const& signalName, uint32_t timeoutMs,
                                           std::function<void()> callback)
{
    TimeoutConfig entry{};
    entry.signalName      = signalName;
    entry.timeoutMs       = timeoutMs;
    entry.isFrameTimeout  = false;
    entry.timeoutCallback = callback;
    m_timeoutTable.push_back(entry);
}

void TimeoutMonitor::registerFrameTimeout(uint8_t channelId, uint32_t frameId, uint32_t timeoutMs,
                                          std::function<void()> callback)
{
    TimeoutConfig entry{};
    entry.channelId       = channelId;
    entry.frameId         = frameId;
    entry.timeoutMs       = timeoutMs;
    entry.isFrameTimeout  = true;
    entry.timeoutCallback = callback;
    m_timeoutTable.push_back(entry);
}

void TimeoutMonitor::notifyRxActivity(std::string const& signalName, uint32_t nowMs)
{
    TimeoutConfig* entry = findSignalEntry(signalName);
    if (entry != nullptr)
    {
        entry->lastRxTimeMs = nowMs;
        entry->expired      = false;
    }
}

void TimeoutMonitor::notifyFrameRx(uint8_t channelId, uint32_t frameId, uint32_t nowMs)
{
    TimeoutConfig* entry = findFrameEntry(channelId, frameId);
    if (entry != nullptr)
    {
        entry->lastRxTimeMs = nowMs;
        entry->expired      = false;
    }
}

void TimeoutMonitor::mainFunction(uint32_t nowMs)
{
    if (!m_initialized)
    {
        return;
    }
    for (auto& entry : m_timeoutTable)
    {
        if (entry.expired || (entry.timeoutMs == 0U))
        {
            continue;
        }
        if ((nowMs - entry.lastRxTimeMs) >= entry.timeoutMs)
        {
            entry.expired = true;
            m_timeoutCount++;
            if (entry.timeoutCallback)
            {
                entry.timeoutCallback();
            }
        }
    }
}

size_t TimeoutMonitor::getEntryCount() const { return m_timeoutTable.size(); }

uint32_t TimeoutMonitor::getTimeoutCount() const { return m_timeoutCount; }

TimeoutConfig* TimeoutMonitor::findSignalEntry(std::string const& signalName)
{
    for (auto& entry : m_timeoutTable)
    {
        if (!entry.isFrameTimeout && (entry.signalName == signalName))
        {
            return &entry;
        }
    }
    return nullptr;
}

TimeoutConfig* TimeoutMonitor::findFrameEntry(uint8_t channelId, uint32_t frameId)
{
    for (auto& entry : m_timeoutTable)
    {
        if (entry.isFrameTimeout && (entry.channelId == channelId) && (entry.frameId == frameId))
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace commgateway
