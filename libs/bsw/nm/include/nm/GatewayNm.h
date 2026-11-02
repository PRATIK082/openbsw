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
 * Gateway Network Management — coordinates sleep/wake across multiple CAN (and,
 * by extension, LIN/Eth) channels. Tracks both ECU-wide state and per-channel
 * state, supports per-channel sleep modes, keep-awake requests, and wakeup
 * propagation. A gateway keeps the whole ECU awake as long as any channel is
 * requesting, kept awake, or awake; allows global sleep only when all channels
 * are ready to sleep and no keep-awake is held.
 */
#pragma once

#include "nm/CanNm.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace nm
{

/// Per-channel sleep mode (independent of ECU-wide state).
enum class ChannelSleepMode : uint8_t
{
    Awake,        ///< full communication, NM active
    Silent,       ///< receive only, no TX (ComM silent)
    Sleep,        ///< channel put to sleep independently
    KeepAwake     ///< forced awake (diagnostics/OTA)
};

bool parseChannelSleepMode(char const* text, ChannelSleepMode& mode);

/// ECU-wide state aggregated from all channels.
enum class EcuState : uint8_t
{
    Off,
    Startup,
    Run,
    PrepareSleep,
    Sleep,
    Wakeup
};

bool parseEcuState(char const* text, EcuState& state);

class GatewayNm
{
public:
    struct ChannelInfo
    {
        CanNm* nm = nullptr;
        ChannelSleepMode mode = ChannelSleepMode::Awake;
        bool keepAwake = false;       ///< per-channel keep-awake hold
        bool requiredSleep = true;    ///< must sleep when ECU sleeps
        bool wakeupPending = false;   ///< wakeup requested on this channel
        NmState lastNmState = NmState::BusSleep;
        uint32_t stateEntryTime = 0U;
    };

    GatewayNm() = default;
    ~GatewayNm() = default;

    GatewayNm(GatewayNm const&) = delete;
    GatewayNm& operator=(GatewayNm const&) = delete;

    // ---- channel registration ----
    bool registerChannel(CanNm* nm);
    size_t getChannelCount() const { return channels_.size(); }
    ChannelInfo const* getChannel(size_t index) const;

    // ---- per-channel sleep mode ----
    bool setChannelSleepMode(size_t index, ChannelSleepMode mode);
    ChannelSleepMode getChannelSleepMode(size_t index) const;
    bool setChannelRequiredSleep(size_t index, bool required);

    // ---- per-channel keep-awake ----
    void channelKeepAwake(size_t index, bool hold);
    bool isChannelKeptAwake(size_t index) const;

    // ---- per-channel wakeup / sleep ----
    void channelWakeup(size_t index);
    void channelSleep(size_t index);
    bool isChannelAwake(size_t index) const;

    // ---- network request propagation (legacy) ----
    void onChannelNetworkRequest(size_t channelIndex);
    void onChannelNetworkRelease(size_t channelIndex);

    // ---- ECU-wide state ----
    EcuState getEcuState() const { return ecuState_; }
    uint32_t getAwakeChannelCount() const;
    bool isEcuAwake() const { return ecuState_ == EcuState::Run || ecuState_ == EcuState::Wakeup || ecuState_ == EcuState::Startup; }

    // ---- periodic processing ----
    void mainFunction(uint32_t nowMs);

    // ---- aggregate queries ----
    bool isNetworkRequested() const;
    bool areAllChannelsReadyToSleep() const;
    bool areAllRequiredChannelsReadyToSleep() const;

    // ---- callbacks ----
    using SleepCallback = std::function<void()>;
    void setGlobalSleepCallback(SleepCallback cb) { globalSleepCallback_ = std::move(cb); }
    using WakeupCallback = std::function<void()>;
    void setGlobalWakeupCallback(WakeupCallback cb) { globalWakeupCallback_ = std::move(cb); }
    using ChannelStateCallback = std::function<void(size_t channelIndex, ChannelSleepMode mode)>;
    void setChannelStateCallback(ChannelStateCallback cb) { channelStateCallback_ = std::move(cb); }

    // ---- tuning ----
    void setPrepareSleepTime(uint32_t ms) { prepareSleepTime_ = ms; }

private:
    void aggregateEcuState(uint32_t nowMs);
    void enforceChannelModes();
    void propagateWakeup(size_t sourceIndex);
    void propagateSleep();

    std::vector<ChannelInfo> channels_;
    EcuState ecuState_ = EcuState::Off;
    uint32_t ecuStateEntryTime_ = 0U;
    uint32_t prepareSleepTime_ = 1500U;
    bool globalSleepAllowed_ = false;
    bool started_ = false;

    SleepCallback globalSleepCallback_;
    WakeupCallback globalWakeupCallback_;
    ChannelStateCallback channelStateCallback_;
};

} // namespace nm