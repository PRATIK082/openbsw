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
 * \ingroup nm
 *
 * Gateway Network Management — coordinates sleep/wake across multiple CAN
 * channels (and, by extension, LIN/Eth channels via adapters). A gateway keeps
 * the whole network awake as long as any single channel has a network request,
 * and allows sleep only when all channels are ready to sleep.
 */
#pragma once

#include "nm/CanNm.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace nm
{

class GatewayNm
{
public:
    using ChannelList = std::vector<CanNm*>;

    GatewayNm() = default;
    ~GatewayNm() = default;

    GatewayNm(GatewayNm const&) = delete;
    GatewayNm& operator=(GatewayNm const&) = delete;

    /// Register a channel's CanNm instance. Returns false on null/duplicate.
    bool registerChannel(CanNm* nm);

    /// Number of registered channels.
    size_t getChannelCount() const { return channels_.size(); }

    /// Notify that a network request was made on a given channel index.
    void onChannelNetworkRequest(size_t channelIndex);

    /// Notify that a network was released on a given channel index.
    void onChannelNetworkRelease(size_t channelIndex);

    /// Periodic processing. nowMs caller-provided tick (ms).
    void mainFunction(uint32_t nowMs);

    /// True if any channel is currently requesting the network.
    bool isNetworkRequested() const;

    /// True if all channels are ready to sleep (or already sleeping).
    bool areAllChannelsReadyToSleep() const;

    /// Callback when gateway decides to allow global sleep.
    using SleepCallback = std::function<void()>;
    void setGlobalSleepCallback(SleepCallback cb) { globalSleepCallback_ = std::move(cb); }

    /// Callback when gateway wakes (any channel activity).
    using WakeupCallback = std::function<void()>;
    void setGlobalWakeupCallback(WakeupCallback cb) { globalWakeupCallback_ = std::move(cb); }

private:
    std::vector<CanNm*> channels_;
    SleepCallback globalSleepCallback_;
    WakeupCallback globalWakeupCallback_;
    bool globalSleepAllowed_ = false;
};

} // namespace nm
