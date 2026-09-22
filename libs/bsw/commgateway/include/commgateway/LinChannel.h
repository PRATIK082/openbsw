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

#include "commgateway/LinHwInterface.h"
#include "commgateway/LinScheduler.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace commgateway
{

/// LIN bus error kinds counted per channel.
enum class LinError : uint8_t
{
    SYNC,
    PARITY,
    CHECKSUM,
    TIMEOUT
};

/// LIN frame configuration: scheduleTimeMs == 0 means event-triggered.
struct LinFrameConfig
{
    uint8_t pid             = 0U;
    uint8_t dlc             = 0U;
    uint32_t scheduleTimeMs = 0U;
    bool isMaster           = true;
    std::string associatedSignal;
};

/// Upper layer notification: (channelId, pid, data, dlc).
using LinUpperCallback = std::function<void(uint8_t, uint8_t, uint8_t*, uint8_t)>;

/**
 * LIN channel manager (master/slave, scheduler, checksum, PID).
 *
 * Owns a LinScheduler for master slots and forwards received frames (after
 * PID parity validation) to the registered upper layer callback with the
 * channel id attached.
 */
class LinChannel
{
public:
    LinChannel() = default;

    void init(LinHwInterface* hw, uint8_t channelId);
    void shutdown();

    void scheduleFrame(LinFrameConfig const& config);
    bool transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc);
    void onFrameReceived(uint8_t pid, uint8_t* data, uint8_t dlc);
    void onFrameTransmitted(uint8_t pid, bool success);
    void registerUpperLayerCallback(LinUpperCallback cb);
    void reportError(LinError error);

    /// Drives the schedule table; call every millisecond with the bus time.
    void mainFunction(uint32_t nowMs);

    void enterSleepMode();
    void wakeup();
    bool isSleeping() const;

    /// LIN PID with parity bits P0/P1 over the 6 bit identifier.
    static uint8_t computePid(uint8_t id);
    static bool isPidValid(uint8_t pid);
    /**
     * LIN checksum: classic (data only) or enhanced (pid + data).
     * \return inverted 8 bit sum with carry handling per LIN 2.x.
     */
    static uint8_t computeChecksum(uint8_t pid, uint8_t const* data, uint8_t dlc, bool enhanced);

    LinFrameConfig const* findFrame(uint8_t pid) const;
    uint8_t getChannelId() const;
    bool isInitialized() const;
    uint32_t getErrorCount(LinError error) const;

private:
    LinHwInterface* m_hw = nullptr;
    uint8_t m_channelId  = 0U;
    bool m_initialized   = false;
    bool m_sleeping      = false;
    LinUpperCallback m_upperCallback;
    std::vector<LinFrameConfig> m_scheduleTable;
    LinScheduler m_scheduler;
    uint32_t m_errorCounts[4U] = {0U, 0U, 0U, 0U};
};

} // namespace commgateway
