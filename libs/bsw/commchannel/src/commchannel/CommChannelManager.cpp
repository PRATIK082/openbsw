/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <commchannel/CommChannelManager.h>

#include <algorithm>
#include <cctype>

namespace commchannel
{

bool parseCommMode(char const* text, CommMode& mode)
{
    if (text == nullptr) return false;
    std::string s(text);
    // uppercase compare simple
    auto upper = [](std::string v) {
        for (char& c : v) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return v;
    };
    s = upper(s);
    if (s == "NO" || s == "NOCOMMUNICATION" || s == "COMM_NO_COMMUNICATION" || s == "NO_COMMUNICATION")
    {
        mode = CommMode::No;
        return true;
    }
    if (s == "SILENT" || s == "SILENTCOMMUNICATION" || s == "COMM_SILENT_COMMUNICATION" || s == "SILENT_COMMUNICATION")
    {
        mode = CommMode::Silent;
        return true;
    }
    if (s == "FULL" || s == "FULLCOMMUNICATION" || s == "COMM_FULL_COMMUNICATION" || s == "FULL_COMMUNICATION")
    {
        mode = CommMode::Full;
        return true;
    }
    return false;
}

bool CommChannelManager::init()
{
    return init(CommChannelManagerConfig{});
}

bool CommChannelManager::init(CommChannelManagerConfig const& config)
{
    if (initialised_) shutdown();
    cfg_ = config;
    if (cfg_.maxChannels == 0U) return false;
    if (cfg_.maxUsersPerChannel == 0U) return false;
    channels_.clear();
    ecuMode_ = ecumode::EcuMode::UNINIT;
    initialised_ = true;
    return true;
}

void CommChannelManager::shutdown()
{
    initialised_ = false;
    channels_.clear();
}

void CommChannelManager::clear()
{
    channels_.clear();
    ecumode::CommStateManagerPhase5::getInstance().clear();
}

bool CommChannelManager::registerChannel(uint8_t channelId)
{
    if (!initialised_) return false;
    if (channels_.size() >= cfg_.maxChannels) 
    {
        // allow existing
        if (channels_.find(channelId) == channels_.end()) return false;
    }
    if (channels_.find(channelId) != channels_.end()) return true; // already
    Channel ch;
    ch.id = channelId;
    channels_[channelId] = ch;
    return true;
}

bool CommChannelManager::removeChannel(uint8_t channelId)
{
    return channels_.erase(channelId) > 0U;
}

bool CommChannelManager::hasChannel(uint8_t channelId) const
{
    return channels_.find(channelId) != channels_.end();
}

std::vector<uint8_t> CommChannelManager::getChannelIds() const
{
    std::vector<uint8_t> ids;
    ids.reserve(channels_.size());
    for (auto const& kv : channels_) ids.push_back(kv.first);
    return ids;
}

Channel const* CommChannelManager::getChannel(uint8_t channelId) const
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return nullptr;
    return &it->second;
}

Channel* CommChannelManager::getChannel(uint8_t channelId)
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return nullptr;
    return &it->second;
}

int8_t CommChannelManager::registerUser(uint8_t channelId)
{
    if (!initialised_) return -1;
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return -1;
    for (uint8_t i = 0U; i < cfg_.maxUsersPerChannel; ++i)
    {
        if (it->second.users.find(i) == it->second.users.end())
        {
            it->second.users.insert(i);
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

bool CommChannelManager::registerUser(uint8_t channelId, uint8_t userId)
{
    if (!initialised_) return false;
    if (userId >= cfg_.maxUsersPerChannel) return false;
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return false;
    it->second.users.insert(userId);
    return true;
}

bool CommChannelManager::unregisterUser(uint8_t channelId, uint8_t userId)
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return false;
    it->second.users.erase(userId);
    it->second.userModes.erase(userId);
    return true;
}

bool CommChannelManager::isUserRegistered(uint8_t channelId, uint8_t userId) const
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return false;
    return it->second.users.find(userId) != it->second.users.end();
}

size_t CommChannelManager::getUserCount(uint8_t channelId) const
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return 0U;
    return it->second.users.size();
}

std::set<uint8_t> CommChannelManager::getUsers(uint8_t channelId) const
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return {};
    return it->second.users;
}

void CommChannelManager::requestCommunication(uint8_t userId, uint8_t channelId, CommMode mode)
{
    if (!initialised_) return;
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return;
    // auto-register user if needed
    if (it->second.users.find(userId) == it->second.users.end())
    {
        if (userId < cfg_.maxUsersPerChannel) it->second.users.insert(userId);
        else return;
    }
    it->second.userModes[userId] = mode;
}

void CommChannelManager::releaseCommunication(uint8_t userId, uint8_t channelId)
{
    if (!initialised_) return;
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return;
    it->second.userModes.erase(userId);
}

void CommChannelManager::requestCommunicationOnChannel(uint8_t channelId, uint8_t userId, CommMode mode)
{
    requestCommunication(userId, channelId, mode);
}

void CommChannelManager::releaseCommunicationOnChannel(uint8_t channelId, uint8_t userId)
{
    releaseCommunication(userId, channelId);
}

CommMode CommChannelManager::getChannelMode(uint8_t channelId) const
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return CommMode::No;
    return it->second.commMode;
}

void CommChannelManager::setChannelMode(uint8_t channelId, CommMode mode)
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return;
    it->second.forcedMode = mode;
}

void CommChannelManager::clearForcedMode(uint8_t channelId)
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return;
    it->second.forcedMode.reset();
}

size_t CommChannelManager::getRequestCount(uint8_t channelId) const
{
    auto it = channels_.find(channelId);
    if (it == channels_.end()) return 0U;
    return it->second.userModes.size();
}

bool CommChannelManager::isChannelRequested(uint8_t channelId) const
{
    return getRequestCount(channelId) > 0U;
}

bool CommChannelManager::isAnyChannelRequested() const
{
    for (auto const& kv : channels_)
    {
        if (!kv.second.userModes.empty()) return true;
    }
    return false;
}

CommMode CommChannelManager::getHighestRequestedMode() const
{
    CommMode highest = CommMode::No;
    for (auto const& kv : channels_)
    {
        for (auto const& um : kv.second.userModes)
        {
            if (um.second > highest) highest = um.second;
        }
        if (kv.second.forcedMode && *kv.second.forcedMode > highest) highest = *kv.second.forcedMode;
    }
    return highest;
}

void CommChannelManager::setEcuMode(ecumode::EcuMode mode)
{
    ecuMode_ = mode;
    if (initialised_)
    {
        ecumode::CommStateManagerPhase5::getInstance().requestEcuMode(mode);
    }
}

ecumode::EcuMode CommChannelManager::getEcuMode() const
{
    return ecuMode_;
}

void CommChannelManager::setModeCommunication(ecumode::EcuMode mode, bool enabled)
{
    ecumode::CommStateManagerPhase5::getInstance().setModeCommunication(mode, enabled);
}

bool CommChannelManager::isModeCommunicationEnabled(ecumode::EcuMode mode) const
{
    return ecumode::CommStateManagerPhase5::getInstance().isModeCommunicationEnabled(mode);
}

CommMode CommChannelManager::aggregateChannel(Channel const& ch, bool commEnabled) const
{
    if (!commEnabled) return CommMode::No;
    CommMode agg = CommMode::No;
    for (auto const& um : ch.userModes)
    {
        if (um.second > agg) agg = um.second;
    }
    if (ch.forcedMode && *ch.forcedMode > agg) agg = *ch.forcedMode;
    return agg;
}

void CommChannelManager::mainFunction(uint32_t nowMs)
{
    if (!initialised_) return;
    auto& csm = ecumode::CommStateManagerPhase5::getInstance();
    csm.mainFunction(nowMs);

    ecumode::EcuMode mode = csm.getCurrentEcuMode();
    ecuMode_ = mode;
    bool commEnabled = csm.isModeCommunicationEnabled(mode);

    for (auto& kv : channels_)
    {
        Channel& ch = kv.second;
        CommMode old = ch.commMode;
        CommMode agg = aggregateChannel(ch, commEnabled);
        ch.commMode = agg;
        ch.lastModeChangeMs = nowMs;
        if (old != agg && modeChangeCallback_)
        {
            modeChangeCallback_(ch.id, old, agg);
        }
    }
    lastTick_ = nowMs;
}

} // namespace commchannel
