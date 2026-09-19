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
#pragma once

#include "canstack/CanRouter.h"
#include "canstack/SignalDb.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace canstack
{

/// One cyclically transmitted signal.
struct TxSignalEntry
{
    std::string signalName;
    uint32_t frameId = 0U;
    uint8_t channelId = 0U;
    uint32_t cycleTimeMs = 0U;
    /// Managed by the scheduler: last transmission time in ms.
    uint32_t lastTxTime = 0U;
    /// Callback to get the current value; when unset, setSignalValue() values are used.
    std::function<double()> valueProvider;
    /// Value set via setSignalValue(); used when no valueProvider is registered.
    double pendingValue = 0.0;
    bool hasPendingValue = false;
};

/**
 * Cycle time based transmission scheduler.
 *
 * mainFunction() is called with a fixed period (assumed 1 ms). For every
 * registered signal the scheduler checks whether (currentTime - lastTxTime) >=
 * cycleTimeMs. When a frame becomes due, all signals of that frame are packed
 * into a zeroed buffer (with the current values of all registered entries of
 * the frame) and transmitted through the transmit function.
 *
 * Multiplexed frames: signals guarded by a multiplex switch value are only
 * packed when the switch signal's current value selects them.
 */
class CanTxScheduler
{
public:
    /// \param db signal database providing the frame layouts
    void setDatabase(SignalDatabase const* db);

    /// \param transmit transmit function (e.g. bound to CanInterface::sendFrame)
    void setTransmitFunction(TransmitFunction transmit);

    /// Registers a cyclic signal entry. cycleTimeMs must be > 0.
    void registerSignal(TxSignalEntry const& entry);

    /// Removes all registered entries.
    void clear();

    /// Stores a value for a registered signal without a value provider.
    void setSignalValue(std::string const& signalName, double value);

    /**
     * Advances the internal millisecond counter by one tick and transmits all
     * due frames. Call periodically (e.g. every 1 ms from the stack's run
     * path).
     */
    void mainFunction();

    /// Overrides the internal time base; when set, mainFunction() reads the
    /// current time from the provider instead of counting ticks.
    void setTimeProvider(std::function<uint32_t()> timeProvider);

    uint32_t getCurrentTimeMs() const;

    std::vector<TxSignalEntry> const& getEntries() const;

private:
    TxSignalEntry* findEntry(std::string const& signalName);
    double resolveValue(TxSignalEntry const& entry) const;
    void processFrameGroup(uint8_t channelId, uint32_t frameId);

    std::vector<TxSignalEntry> m_txSignals;
    SignalDatabase const* m_signalDb = nullptr;
    TransmitFunction m_transmit;
    std::function<uint32_t()> m_timeProvider;
    uint32_t m_currentTimeMs = 0U;
};

} // namespace canstack
