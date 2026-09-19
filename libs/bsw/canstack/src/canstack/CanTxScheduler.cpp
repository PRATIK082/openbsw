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
#include "canstack/CanTxScheduler.h"
#include "canstack/CanFrame.h"
#include "canstack/CanStackLogger.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace canstack
{
namespace logger = ::util::logger;

void CanTxScheduler::setDatabase(SignalDatabase const* db) { m_signalDb = db; }

void CanTxScheduler::setTransmitFunction(TransmitFunction transmit)
{
    m_transmit = std::move(transmit);
}

void CanTxScheduler::registerSignal(TxSignalEntry const& entry)
{
    if (entry.cycleTimeMs == 0U)
    {
        logger::Logger::error(
            logger::CANSTACK, "Tx scheduler: cycle time of %s is zero", entry.signalName.c_str());
        return;
    }

    TxSignalEntry stored = entry;
    stored.lastTxTime    = getCurrentTimeMs();

    for (auto& existing : m_txSignals)
    {
        if (existing.signalName == entry.signalName)
        {
            existing = stored;
            return;
        }
    }

    m_txSignals.push_back(stored);
}

void CanTxScheduler::clear() { m_txSignals.clear(); }

void CanTxScheduler::setSignalValue(std::string const& signalName, double const value)
{
    TxSignalEntry* entry = findEntry(signalName);
    if (entry == nullptr)
    {
        logger::Logger::warn(
            logger::CANSTACK, "Tx scheduler: %s is not registered", signalName.c_str());
        return;
    }

    entry->pendingValue    = value;
    entry->hasPendingValue = true;
}

void CanTxScheduler::mainFunction()
{
    if (m_timeProvider)
    {
        m_currentTimeMs = m_timeProvider();
    }
    else
    {
        ++m_currentTimeMs;
    }

    if (m_txSignals.empty() || !m_transmit || (m_signalDb == nullptr))
    {
        return;
    }

    // Process every registered frame once; a frame is due when any of its
    // cyclic signals elapsed its cycle time.
    for (size_t i = 0U; i < m_txSignals.size(); ++i)
    {
        bool const alreadyProcessed = std::any_of(
            m_txSignals.begin(),
            m_txSignals.begin() + static_cast<std::ptrdiff_t>(i),
            [this, &entry = m_txSignals[i]](TxSignalEntry const& other) {
                return (other.channelId == entry.channelId) && (other.frameId == entry.frameId);
            });
        if (alreadyProcessed)
        {
            continue;
        }

        processFrameGroup(m_txSignals[i].channelId, m_txSignals[i].frameId);
    }
}

void CanTxScheduler::setTimeProvider(std::function<uint32_t()> timeProvider)
{
    m_timeProvider = std::move(timeProvider);
}

uint32_t CanTxScheduler::getCurrentTimeMs() const { return m_currentTimeMs; }

std::vector<TxSignalEntry> const& CanTxScheduler::getEntries() const { return m_txSignals; }

TxSignalEntry* CanTxScheduler::findEntry(std::string const& signalName)
{
    for (auto& entry : m_txSignals)
    {
        if (entry.signalName == signalName)
        {
            return &entry;
        }
    }
    return nullptr;
}

double CanTxScheduler::resolveValue(TxSignalEntry const& entry) const
{
    if (entry.valueProvider)
    {
        return entry.valueProvider();
    }
    return entry.pendingValue;
}

void CanTxScheduler::processFrameGroup(uint8_t const channelId, uint32_t const frameId)
{
    uint32_t const now = getCurrentTimeMs();

    // Due check: any signal of the frame elapsed its cycle time.
    bool due = false;
    for (auto const& entry : m_txSignals)
    {
        if ((entry.channelId != channelId) || (entry.frameId != frameId))
        {
            continue;
        }
        if ((now - entry.lastTxTime) >= entry.cycleTimeMs)
        {
            due = true;
            break;
        }
    }

    if (!due)
    {
        return;
    }

    FrameConfig const* frame = m_signalDb->getFrameByChannelAndId(channelId, frameId);
    if (frame == nullptr)
    {
        logger::Logger::debug(logger::CANSTACK, "Tx scheduler: frame 0x%lx unknown", frameId);
        for (auto& entry : m_txSignals)
        {
            if ((entry.channelId == channelId) && (entry.frameId == frameId))
            {
                entry.lastTxTime = now;
            }
        }
        return;
    }

    uint8_t data[CanFrame::MAX_DATA_LENGTH] = {};

    // Resolve the multiplexer switch value of the frame, if one is registered.
    double switchValue = 0.0;
    bool hasSwitch = false;
    for (auto const& signal : frame->signals)
    {
        if (!signal.isMultiplexerSwitch)
        {
            continue;
        }
        TxSignalEntry const* switchEntry = nullptr;
        for (auto const& entry : m_txSignals)
        {
            if ((entry.channelId == channelId) && (entry.frameId == frameId)
                && (entry.signalName == signal.signalName))
            {
                switchEntry = &entry;
                break;
            }
        }
        if (switchEntry != nullptr)
        {
            switchValue = resolveValue(*switchEntry);
            hasSwitch   = true;
        }
        break;
    }

    // Pack every registered signal of the frame.
    for (auto const& entry : m_txSignals)
    {
        if ((entry.channelId != channelId) || (entry.frameId != frameId))
        {
            continue;
        }

        SignalConfig const* signal = nullptr;
        for (auto const& candidate : frame->signals)
        {
            if (candidate.signalName == entry.signalName)
            {
                signal = &candidate;
                break;
            }
        }

        if (signal == nullptr)
        {
            continue;
        }

        // Multiplexed signals are only packed while the switch selects them.
        if (hasSwitch && (signal->multiplexValue >= 0) && !signal->isMultiplexerSwitch)
        {
            if (static_cast<int64_t>(switchValue) != static_cast<int64_t>(signal->multiplexValue))
            {
                continue;
            }
        }

        m_signalDb->packSignal(data, *signal, resolveValue(entry));
    }

    (void)m_transmit(channelId, frameId, frame->dlc, data);

    for (auto& entry : m_txSignals)
    {
        if ((entry.channelId == channelId) && (entry.frameId == frameId))
        {
            entry.lastTxTime = now;
        }
    }
}

} // namespace canstack
