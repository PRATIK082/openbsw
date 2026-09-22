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
#include <string>
#include <vector>

namespace commgateway
{

/// One supervised signal or frame with its timeout budget.
struct TimeoutConfig
{
    std::string signalName;
    uint32_t timeoutMs   = 0U;
    uint32_t lastRxTimeMs = 0U;
    /// true = frame-level (channelId + frameId), false = signal-level.
    bool isFrameTimeout  = false;
    uint8_t channelId    = 0U;
    uint32_t frameId     = 0U;
    std::function<void()> timeoutCallback;
    bool expired         = false;
};

/**
 * Rx frame/signal timeout detection.
 *
 * Protocol layers report activity via notifyRxActivity()/notifyFrameRx();
 * mainFunction() fires each expired entry's callback once until activity
 * re-arms it. Call mainFunction() every 10 ms.
 */
class TimeoutMonitor
{
public:
    TimeoutMonitor() = default;

    void init();
    void shutdown();
    void clear();

    void registerSignalTimeout(std::string const& signalName, uint32_t timeoutMs,
                               std::function<void()> callback);
    void registerFrameTimeout(uint8_t channelId, uint32_t frameId, uint32_t timeoutMs,
                              std::function<void()> callback);
    void notifyRxActivity(std::string const& signalName, uint32_t nowMs);
    void notifyFrameRx(uint8_t channelId, uint32_t frameId, uint32_t nowMs);

    /// Checks all entries against nowMs; call every 10 ms.
    void mainFunction(uint32_t nowMs);

    size_t getEntryCount() const;
    uint32_t getTimeoutCount() const;

private:
    TimeoutConfig* findSignalEntry(std::string const& signalName);
    TimeoutConfig* findFrameEntry(uint8_t channelId, uint32_t frameId);

    std::vector<TimeoutConfig> m_timeoutTable;
    uint32_t m_timeoutCount = 0U;
    bool m_initialized      = false;
};

} // namespace commgateway
