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
 * \ingroup commchannel
 *
 * CommChannelManager - manages communication channels and user requests.
 *
 * Alternative naming for the AUTOSAR Communication Manager (ComM). Each
 * channel has an id, a set of users and an aggregated communication mode
 * (FULL / SILENT / NO). Users request/release communication per
 * (user, channel); the channel mode is the highest request among its users.
 * Tick-driven via mainFunction().
 */
#pragma once

#include <ecumode/EcuMode.h>
#include <ecumode/CommStateManagerPhase5.h>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace commchannel
{

/// Communication mode of a channel/user.
enum class CommMode : uint8_t
{
    No = 0U,
    Silent = 1U,
    Full = 2U
};

/// AUTOSAR-style constant names (avoiding ComM prefixes).
static constexpr CommMode COMM_NO_COMMUNICATION = CommMode::No;
static constexpr CommMode COMM_SILENT_COMMUNICATION = CommMode::Silent;
static constexpr CommMode COMM_FULL_COMMUNICATION = CommMode::Full;

/// Parses a CommMode name; returns false on unknown text.
bool parseCommMode(char const* text, CommMode& mode);

struct CommChannelManagerConfig
{
    uint8_t maxChannels = 16U;
    uint8_t maxUsersPerChannel = 8U;
};

/// A single communication channel.
struct Channel
{
    uint8_t id = 0U;
    std::set<uint8_t> users;                 ///< registered users
    std::map<uint8_t, CommMode> userModes;   ///< per-user requested modes
    std::optional<CommMode> forcedMode;      ///< optional forced mode
    CommMode commMode = CommMode::No;        ///< aggregated mode
    uint32_t lastModeChangeMs = 0U;
};

class CommChannelManager
{
public:
    using ModeChangeCallback =
        std::function<void(uint8_t channelId, CommMode oldMode, CommMode newMode)>;

    CommChannelManager() = default;
    ~CommChannelManager() = default;

    CommChannelManager(CommChannelManager const&) = delete;
    CommChannelManager& operator=(CommChannelManager const&) = delete;

    // ---- lifecycle ----
    bool init();
    bool init(CommChannelManagerConfig const& config);
    void shutdown();
    void clear();

    bool isInitialised() const { return initialised_; }

    CommChannelManagerConfig const& getConfig() const { return cfg_; }

    // ---- channel management ----
    bool registerChannel(uint8_t channelId);
    bool addChannel(uint8_t channelId) { return registerChannel(channelId); }
    bool removeChannel(uint8_t channelId);
    bool hasChannel(uint8_t channelId) const;

    size_t getChannelCount() const { return channels_.size(); }
    std::vector<uint8_t> getChannelIds() const;

    Channel const* getChannel(uint8_t channelId) const;
    Channel* getChannel(uint8_t channelId);

    // ---- user management ----
    int8_t registerUser(uint8_t channelId);
    bool registerUser(uint8_t channelId, uint8_t userId);
    bool addUser(uint8_t channelId, uint8_t userId) { return registerUser(channelId, userId); }
    bool addUser(uint8_t channelId) { return registerUser(channelId) >= 0; }
    bool unregisterUser(uint8_t channelId, uint8_t userId);
    bool isUserRegistered(uint8_t channelId, uint8_t userId) const;

    size_t getUserCount(uint8_t channelId) const;
    std::set<uint8_t> getUsers(uint8_t channelId) const;

    // ---- user request API (AUTOSAR order: user, channel) ----
    void requestCommunication(uint8_t userId, uint8_t channelId,
                             CommMode mode = CommMode::Full);
    void releaseCommunication(uint8_t userId, uint8_t channelId);

    // aliases
    void requestComMode(uint8_t userId, uint8_t channelId,
                        CommMode mode = CommMode::Full)
    {
        requestCommunication(userId, channelId, mode);
    }
    void releaseComMode(uint8_t userId, uint8_t channelId)
    {
        releaseCommunication(userId, channelId);
    }
    void request(uint8_t userId, uint8_t channelId, CommMode mode = CommMode::Full)
    {
        requestCommunication(userId, channelId, mode);
    }
    void release(uint8_t userId, uint8_t channelId)
    {
        releaseCommunication(userId, channelId);
    }

    // channel-first convenience
    void requestCommunicationOnChannel(uint8_t channelId, uint8_t userId,
                                      CommMode mode = CommMode::Full);
    void releaseCommunicationOnChannel(uint8_t channelId, uint8_t userId);

    // ---- channel mode management ----
    CommMode getChannelMode(uint8_t channelId) const;
    CommMode getChannelCommMode(uint8_t channelId) const
    {
        return getChannelMode(channelId);
    }
    CommMode getCommMode(uint8_t channelId) const { return getChannelMode(channelId); }

    void setChannelMode(uint8_t channelId, CommMode mode);
    void setChannelCommMode(uint8_t channelId, CommMode mode)
    {
        setChannelMode(channelId, mode);
    }
    void setCommMode(uint8_t channelId, CommMode mode) { setChannelMode(channelId, mode); }
    void clearForcedMode(uint8_t channelId);

    // ---- aggregation helpers ----
    size_t getRequestCount(uint8_t channelId) const;
    bool isChannelRequested(uint8_t channelId) const;
    bool isAnyChannelRequested() const;
    CommMode getHighestRequestedMode() const;

    // ---- EcuMode tie-in ----
    void setEcuMode(ecumode::EcuMode mode);
    ecumode::EcuMode getEcuMode() const;
    void setModeCommunication(ecumode::EcuMode mode, bool enabled);
    bool isModeCommunicationEnabled(ecumode::EcuMode mode) const;

    // ---- periodic processing ----
    void mainFunction(uint32_t nowMs);

    // ---- callbacks ----
    void setModeChangeCallback(ModeChangeCallback cb) { modeChangeCallback_ = std::move(cb); }

private:
    CommMode aggregateChannel(Channel const& ch, bool commEnabled) const;

    CommChannelManagerConfig cfg_{};
    bool initialised_ = false;

    std::map<uint8_t, Channel> channels_;

    ecumode::EcuMode ecuMode_ = ecumode::EcuMode::UNINIT;

    ModeChangeCallback modeChangeCallback_;

    uint32_t lastTick_ = 0U;
};

// short alias
using ChannelManager = CommChannelManager;

} // namespace commchannel

namespace commmanager = commchannel;
