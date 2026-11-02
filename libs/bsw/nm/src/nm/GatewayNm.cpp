/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "nm/GatewayNm.h"

#include <algorithm>

namespace nm
{

bool GatewayNm::registerChannel(CanNm* nm)
{
    if (nm == nullptr) return false;
    if (std::find(channels_.begin(), channels_.end(), nm) != channels_.end()) return false;
    channels_.push_back(nm);
    return true;
}

void GatewayNm::onChannelNetworkRequest(size_t channelIndex)
{
    if (channelIndex >= channels_.size()) return;
    // propagate request to all channels to wake the whole gateway network
    for (auto* c : channels_)
    {
        if (c != nullptr) c->networkRequest();
    }
    if (globalWakeupCallback_) globalWakeupCallback_();
}

void GatewayNm::onChannelNetworkRelease(size_t channelIndex)
{
    if (channelIndex >= channels_.size()) return;
    // release only on the requesting channel; others keep their own state
    if (channels_[channelIndex] != nullptr) channels_[channelIndex]->networkRelease();
}

bool GatewayNm::isNetworkRequested() const
{
    for (auto* c : channels_)
        if (c != nullptr && c->isNetworkRequested()) return true;
    return false;
}

bool GatewayNm::areAllChannelsReadyToSleep() const
{
    if (channels_.empty()) return true;
    for (auto* c : channels_)
    {
        if (c == nullptr) continue;
        NmState s = c->getState();
        if (s != NmState::ReadySleep && s != NmState::PrepareBusSleep && s != NmState::BusSleep)
            return false;
        if (c->isNetworkRequested()) return false;
    }
    return true;
}

void GatewayNm::mainFunction(uint32_t nowMs)
{
    if (channels_.empty()) return;

    // If any channel is requesting, keep all channels awake.
    bool anyRequested = isNetworkRequested();
    if (anyRequested)
    {
        for (auto* c : channels_)
            if (c != nullptr && !c->isNetworkRequested()) c->networkRequest();
        if (globalWakeupCallback_ && !globalSleepAllowed_)
        {
            // only fire once on transition from sleep-allowed to awake
        }
        globalSleepAllowed_ = false;
    }

    // Run each channel's NM state machine.
    for (auto* c : channels_)
        if (c != nullptr) c->mainFunction(nowMs);

    // If all channels ready to sleep and none requesting, allow global sleep.
    bool allReady = areAllChannelsReadyToSleep();
    if (allReady && !globalSleepAllowed_)
    {
        globalSleepAllowed_ = true;
        if (globalSleepCallback_) globalSleepCallback_();
    }
    else if (!allReady)
    {
        globalSleepAllowed_ = false;
    }
}

} // namespace nm
