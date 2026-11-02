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
#include <string>

namespace nm
{

bool parseChannelSleepMode(char const* text, ChannelSleepMode& mode)
{
    if (text == nullptr) return false;
    std::string s(text);
    if (s == "Awake") { mode = ChannelSleepMode::Awake; return true; }
    if (s == "Silent") { mode = ChannelSleepMode::Silent; return true; }
    if (s == "Sleep") { mode = ChannelSleepMode::Sleep; return true; }
    if (s == "KeepAwake") { mode = ChannelSleepMode::KeepAwake; return true; }
    return false;
}

bool parseEcuState(char const* text, EcuState& state)
{
    if (text == nullptr) return false;
    std::string s(text);
    if (s == "Off") { state = EcuState::Off; return true; }
    if (s == "Startup") { state = EcuState::Startup; return true; }
    if (s == "Run") { state = EcuState::Run; return true; }
    if (s == "PrepareSleep") { state = EcuState::PrepareSleep; return true; }
    if (s == "Sleep") { state = EcuState::Sleep; return true; }
    if (s == "Wakeup") { state = EcuState::Wakeup; return true; }
    return false;
}

bool GatewayNm::registerChannel(CanNm* nm)
{
    if (nm == nullptr) return false;
    for (auto const& c : channels_)
        if (c.nm == nm) return false;
    ChannelInfo ci;
    ci.nm = nm;
    ci.lastNmState = nm->getState();
    channels_.push_back(ci);
    if (ecuState_ == EcuState::Off) { ecuState_ = EcuState::Startup; ecuStateEntryTime_ = 0U; started_ = true; }
    return true;
}

GatewayNm::ChannelInfo const* GatewayNm::getChannel(size_t index) const
{
    if (index >= channels_.size()) return nullptr;
    return &channels_[index];
}

bool GatewayNm::setChannelSleepMode(size_t index, ChannelSleepMode mode)
{
    if (index >= channels_.size()) return false;
    channels_[index].mode = mode;
    if (channels_[index].nm != nullptr)
    {
        if (mode == ChannelSleepMode::Silent) channels_[index].nm->setCommMode(CommMode::SilentCommunication);
        else if (mode == ChannelSleepMode::Sleep) channels_[index].nm->setCommMode(CommMode::NoCommunication);
        else channels_[index].nm->setCommMode(CommMode::FullCommunication);
    }
    if (channelStateCallback_) channelStateCallback_(index, mode);
    return true;
}

ChannelSleepMode GatewayNm::getChannelSleepMode(size_t index) const
{
    if (index >= channels_.size()) return ChannelSleepMode::Awake;
    return channels_[index].mode;
}

bool GatewayNm::setChannelRequiredSleep(size_t index, bool required)
{
    if (index >= channels_.size()) return false;
    channels_[index].requiredSleep = required;
    return true;
}

void GatewayNm::channelKeepAwake(size_t index, bool hold)
{
    if (index >= channels_.size()) return;
    channels_[index].keepAwake = hold;
    if (hold)
    {
        // keep-awake channel forces ECU awake
        if (ecuState_ == EcuState::Sleep || ecuState_ == EcuState::PrepareSleep)
        {
            ecuState_ = EcuState::Wakeup;
            ecuStateEntryTime_ = 0U;
            if (globalWakeupCallback_) globalWakeupCallback_();
            propagateWakeup(index);
        }
        if (channels_[index].nm != nullptr) channels_[index].nm->networkRequest();
    }
}

bool GatewayNm::isChannelKeptAwake(size_t index) const
{
    if (index >= channels_.size()) return false;
    return channels_[index].keepAwake;
}

void GatewayNm::channelWakeup(size_t index)
{
    if (index >= channels_.size()) return;
    channels_[index].wakeupPending = true;
    channels_[index].mode = ChannelSleepMode::Awake;
    setChannelSleepMode(index, ChannelSleepMode::Awake);
    if (channels_[index].nm != nullptr) channels_[index].nm->networkRequest();
    // wake ECU if sleeping
    if (ecuState_ == EcuState::Sleep || ecuState_ == EcuState::PrepareSleep)
    {
        ecuState_ = EcuState::Run;
        ecuStateEntryTime_ = 0U;
        if (globalWakeupCallback_) globalWakeupCallback_();
    }
    propagateWakeup(index);
}

void GatewayNm::channelSleep(size_t index)
{
    if (index >= channels_.size()) return;
    setChannelSleepMode(index, ChannelSleepMode::Sleep);
    if (channels_[index].nm != nullptr) channels_[index].nm->networkRelease();
}

bool GatewayNm::isChannelAwake(size_t index) const
{
    if (index >= channels_.size()) return false;
    return channels_[index].mode != ChannelSleepMode::Sleep;
}

uint32_t GatewayNm::getAwakeChannelCount() const
{
    uint32_t n = 0U;
    for (size_t i = 0U; i < channels_.size(); ++i)
        if (isChannelAwake(i)) ++n;
    return n;
}

void GatewayNm::onChannelNetworkRequest(size_t channelIndex)
{
    if (channelIndex >= channels_.size()) return;
    // propagate request to all awake channels
    for (auto& c : channels_)
    {
        if (c.nm == nullptr) continue;
        if (c.mode == ChannelSleepMode::Sleep) continue; // sleeping channels stay asleep
        c.nm->networkRequest();
    }
    if (ecuState_ == EcuState::Sleep || ecuState_ == EcuState::PrepareSleep)
    {
        ecuState_ = EcuState::Run;
        ecuStateEntryTime_ = 0U;
    }
    if (globalWakeupCallback_) globalWakeupCallback_();
    propagateWakeup(channelIndex);
}

void GatewayNm::onChannelNetworkRelease(size_t channelIndex)
{
    if (channelIndex >= channels_.size()) return;
    if (channels_[channelIndex].nm != nullptr) channels_[channelIndex].nm->networkRelease();
}

bool GatewayNm::isNetworkRequested() const
{
    for (auto const& c : channels_)
        if (c.nm != nullptr && c.nm->isNetworkRequested()) return true;
    for (auto const& c : channels_)
        if (c.keepAwake) return true;
    return false;
}

bool GatewayNm::areAllChannelsReadyToSleep() const
{
    if (channels_.empty()) return true;
    for (auto const& c : channels_)
    {
        if (c.nm == nullptr) continue;
        if (c.keepAwake) return false;
        if (c.mode == ChannelSleepMode::KeepAwake) return false;
        NmState s = c.nm->getState();
        if (s != NmState::ReadySleep && s != NmState::PrepareBusSleep && s != NmState::BusSleep)
            return false;
        if (c.nm->isNetworkRequested()) return false;
    }
    return true;
}

bool GatewayNm::areAllRequiredChannelsReadyToSleep() const
{
    if (channels_.empty()) return true;
    for (auto const& c : channels_)
    {
        if (!c.requiredSleep) continue;
        if (c.nm == nullptr) continue;
        if (c.keepAwake) return false;
        NmState s = c.nm->getState();
        if (s != NmState::ReadySleep && s != NmState::PrepareBusSleep && s != NmState::BusSleep)
            return false;
        if (c.nm->isNetworkRequested()) return false;
    }
    return true;
}

void GatewayNm::propagateWakeup(size_t sourceIndex)
{
    for (size_t i = 0U; i < channels_.size(); ++i)
    {
        if (i == sourceIndex) continue;
        if (channels_[i].mode == ChannelSleepMode::Sleep) continue; // sleeping channels stay asleep unless woken explicitly
        if (channels_[i].nm != nullptr)
        {
            channels_[i].nm->networkRequest();
        }
    }
    if (globalWakeupCallback_) globalWakeupCallback_();
}

void GatewayNm::propagateSleep()
{
    for (auto& c : channels_)
    {
        if (!c.requiredSleep) continue;
        if (c.mode == ChannelSleepMode::KeepAwake) continue;
        if (c.nm != nullptr) c.nm->networkRelease();
        c.mode = ChannelSleepMode::Sleep;
        if (channelStateCallback_) {} // notify via separate? keep simple
    }
}

void GatewayNm::enforceChannelModes()
{
    for (size_t i = 0U; i < channels_.size(); ++i)
    {
        auto& c = channels_[i];
        if (c.nm == nullptr) continue;
        switch (c.mode)
        {
            case ChannelSleepMode::Awake:
                c.nm->setCommMode(CommMode::FullCommunication);
                break;
            case ChannelSleepMode::Silent:
                c.nm->setCommMode(CommMode::SilentCommunication);
                break;
            case ChannelSleepMode::Sleep:
                c.nm->setCommMode(CommMode::NoCommunication);
                c.nm->networkRelease();
                break;
            case ChannelSleepMode::KeepAwake:
                c.nm->setCommMode(CommMode::FullCommunication);
                c.nm->networkRequest();
                break;
        }
    }
}

void GatewayNm::aggregateEcuState(uint32_t nowMs)
{
    if (!started_) return;

    // update lastNmState tracking
    for (auto& c : channels_)
    {
        if (c.nm == nullptr) continue;
        NmState s = c.nm->getState();
        c.lastNmState = s;
    }

    // wakeup pending handling
    bool anyWakeupPending = false;
    for (auto& c : channels_) if (c.wakeupPending) { anyWakeupPending = true; c.wakeupPending = false; }

    bool anyKeepAwake = false;
    for (auto const& c : channels_) if (c.keepAwake || c.mode == ChannelSleepMode::KeepAwake) anyKeepAwake = true;

    bool anyRequested = isNetworkRequested();

    switch (ecuState_)
    {
        case EcuState::Off:
            ecuState_ = EcuState::Startup;
            ecuStateEntryTime_ = nowMs;
            break;

        case EcuState::Startup:
            ecuState_ = EcuState::Run;
            ecuStateEntryTime_ = nowMs;
            break;

        case EcuState::Run:
            if (anyWakeupPending) { ecuStateEntryTime_ = nowMs; break; }
            if (anyKeepAwake || anyRequested) { ecuStateEntryTime_ = nowMs; break; }
            if (areAllRequiredChannelsReadyToSleep())
            {
                ecuState_ = EcuState::PrepareSleep;
                ecuStateEntryTime_ = nowMs;
            }
            break;

        case EcuState::PrepareSleep:
            if (anyKeepAwake || anyRequested || anyWakeupPending)
            {
                ecuState_ = EcuState::Run;
                ecuStateEntryTime_ = nowMs;
                break;
            }
            if (areAllRequiredChannelsReadyToSleep())
            {
                if ((nowMs - ecuStateEntryTime_) >= prepareSleepTime_)
                {
                    ecuState_ = EcuState::Sleep;
                    ecuStateEntryTime_ = nowMs;
                    propagateSleep();
                    if (globalSleepCallback_) globalSleepCallback_();
                    globalSleepAllowed_ = true;
                }
            }
            else
            {
                ecuState_ = EcuState::Run;
                ecuStateEntryTime_ = nowMs;
            }
            break;

        case EcuState::Sleep:
            if (anyKeepAwake || anyRequested || anyWakeupPending)
            {
                ecuState_ = EcuState::Wakeup;
                ecuStateEntryTime_ = nowMs;
                if (globalWakeupCallback_) globalWakeupCallback_();
            }
            break;

        case EcuState::Wakeup:
            // wake required channels
            for (auto& c : channels_)
            {
                if (!c.requiredSleep) continue;
                if (c.nm != nullptr)
                {
                    c.mode = ChannelSleepMode::Awake;
                    c.nm->setCommMode(CommMode::FullCommunication);
                    c.nm->networkRequest();
                }
            }
            ecuState_ = EcuState::Run;
            ecuStateEntryTime_ = nowMs;
            break;
    }
}

void GatewayNm::mainFunction(uint32_t nowMs)
{
    if (channels_.empty()) return;
    if (!started_) { started_ = true; ecuState_ = EcuState::Startup; ecuStateEntryTime_ = nowMs; }

    enforceChannelModes();

    // run each channel NM
    for (auto& c : channels_)
    {
        if (c.nm == nullptr) continue;
        // sleeping channels: don't run NM (keep released)
        if (c.mode == ChannelSleepMode::Sleep) continue;
        c.nm->mainFunction(nowMs);
    }

    aggregateEcuState(nowMs);
}

} // namespace nm