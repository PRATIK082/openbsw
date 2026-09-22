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
#include <vector>

namespace commgateway
{

/// One master schedule slot: transmit the header of pid every cycleTimeMs.
/// cycleTimeMs == 0 marks an event-triggered (spontaneous) frame.
struct LinScheduleEntry
{
    uint8_t pid          = 0U;
    uint8_t dlc          = 0U;
    uint32_t cycleTimeMs = 0U;
    bool enabled         = true;
    uint32_t lastTxTimeMs = 0U;
};

/// Master transmit hook: (pid, dlc) -> header accepted.
using LinHeaderTransmitFn = std::function<bool(uint8_t, uint8_t)>;

/**
 * LIN master schedule table executor.
 *
 * Time is an injected millisecond tick so the scheduler stays OS free and
 * unit testable; comparisons are wrap safe ((now - last) >= period).
 */
class LinScheduler
{
public:
    void addEntry(LinScheduleEntry const& entry);
    bool removeEntry(uint8_t pid);
    void clear();

    /// Fires due cyclic entries via transmit; call every millisecond.
    void tick(uint32_t nowMs, LinHeaderTransmitFn transmit);

    size_t getEntryCount() const;
    LinScheduleEntry const* getEntry(uint8_t pid) const;

private:
    std::vector<LinScheduleEntry> m_entries;
};

} // namespace commgateway
