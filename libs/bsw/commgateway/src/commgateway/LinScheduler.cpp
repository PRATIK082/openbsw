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
#include "commgateway/LinScheduler.h"

namespace commgateway
{

void LinScheduler::addEntry(LinScheduleEntry const& entry)
{
    for (auto& existing : m_entries)
    {
        if (existing.pid == entry.pid)
        {
            existing = entry;
            return;
        }
    }
    m_entries.push_back(entry);
}

bool LinScheduler::removeEntry(uint8_t pid)
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
    {
        if (it->pid == pid)
        {
            m_entries.erase(it);
            return true;
        }
    }
    return false;
}

void LinScheduler::clear() { m_entries.clear(); }

void LinScheduler::tick(uint32_t nowMs, LinHeaderTransmitFn transmit)
{
    if (!transmit)
    {
        return;
    }
    for (auto& entry : m_entries)
    {
        if (!entry.enabled || (entry.cycleTimeMs == 0U))
        {
            continue;
        }
        if ((nowMs - entry.lastTxTimeMs) >= entry.cycleTimeMs)
        {
            if (transmit(entry.pid, entry.dlc))
            {
                entry.lastTxTimeMs = nowMs;
            }
        }
    }
}

size_t LinScheduler::getEntryCount() const { return m_entries.size(); }

LinScheduleEntry const* LinScheduler::getEntry(uint8_t pid) const
{
    for (auto const& entry : m_entries)
    {
        if (entry.pid == pid)
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace commgateway
