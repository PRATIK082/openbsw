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
 * \ingroup ecumode
 */
#pragma once

#include <commgateway/CommStateManager.h>
#include <ecumode/EcuMode.h>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ecumode
{

/**
 * ECU mode manager with per-mode frame filter policies (Phase 5).
 *
 * Extends commgateway::CommStateManager with an ECU-wide mode state machine.
 * Mode requests are latched as pending and applied in mainFunction(); the
 * effective filter for a frame is the per-mode default unless a
 * (frameId, channelType) exception exists. Unknown modes default to ALLOW.
 *
 * Mode actions are dispatched to caller-registered handlers only; this class
 * never calls other BSW singletons directly.
 */
class CommStateManagerPhase5 : public ::commgateway::CommStateManager
{
public:
    static CommStateManagerPhase5& getInstance();

    void requestEcuMode(EcuMode mode);
    EcuMode getCurrentEcuMode() const;

    void setDefaultPolicy(EcuMode mode, FrameFilterPolicy policy);
    void
    addException(EcuMode mode, uint32_t frameId, uint8_t channelType, FrameFilterPolicy policy);
    void clearPolicies();

    FrameFilterPolicy getFrameFilterPolicy(uint32_t frameId, uint8_t channelType) const;
    bool isFrameAllowed(uint32_t frameId, uint8_t channelType, bool isTx) const;

    using ModeChangeCallback = std::function<void(EcuMode from, EcuMode to)>;
    void setModeChangeCallback(ModeChangeCallback callback);

    using ActionHandler = std::function<void(EcuMode)>;
    void registerActionHandler(std::string const& name, ActionHandler handler);
    void setModeActions(EcuMode mode, std::vector<std::string> const& actions);

    /// Loads modes/actions from a JSON document; false + error text on malformed input.
    bool loadConfig(std::string const& jsonText, std::string& error);

    void clear();

    /// Wakeup source registry (Phase-5/9): register a checker for a wakeup source id.
    using WakeupChecker = std::function<bool()>;
    void registerWakeupSource(uint32_t id, WakeupChecker checker);
    void unregisterWakeupSource(uint32_t id);
    bool isWakeupSourceActive(uint32_t id) const;
    std::vector<uint32_t> getActiveWakeupSources() const;
    /// Returns true if at least one registered wakeup source is currently active.
    bool hasActiveWakeup() const;
    /// Returns the id of the first active wakeup source, or 0 if none.
    uint32_t getWakeupReason() const;
    /// Propagates an active wakeup to ECU mode: if any wakeup source is active and
    /// the current mode is SLEEP or STANDBY, requests a transition to NORMAL.
    void processWakeupSources();

    /// Run-request management (BswM-like): clients request/release run; ECU stays awake while any request active.
    void requestRun(std::string const& client);
    void releaseRun(std::string const& client);
    bool hasRunRequests() const;
    size_t getRunRequestCount() const;

    /// Mode-dependent communication enable flag (per ECU mode).
    void setModeCommunication(EcuMode mode, bool enabled);
    bool isModeCommunicationEnabled(EcuMode mode) const;

    // Hides (does not override) the non-virtual base mainFunction; applies any
    // pending ECU mode after running the base channel state machine.
    // NOLINTNEXTLINE: base mainFunction is non-virtual, hiding is intended.
    void mainFunction(uint32_t nowMs);

    uint32_t getBlockCount() const;

private:
    CommStateManagerPhase5() = default;

    using ExceptionKey = std::pair<uint32_t, uint8_t>;

    EcuMode m_currentMode = EcuMode::UNINIT;
    bool m_hasPending     = false;
    EcuMode m_pendingMode = EcuMode::UNINIT;

    std::map<EcuMode, FrameFilterPolicy> m_defaultPolicies;
    std::map<std::pair<EcuMode, ExceptionKey>, FrameFilterPolicy> m_exceptions;
    std::map<EcuMode, std::vector<std::string>> m_modeActions;
    std::map<std::string, ActionHandler> m_actionHandlers;

    ModeChangeCallback m_modeChangeCallback;
    mutable uint32_t m_blockCount = 0U;

    std::map<uint32_t, WakeupChecker> m_wakeupSources{};
    std::set<std::string> m_runRequests{};
    std::map<EcuMode, bool> m_modeCommunication{};
};

} // namespace ecumode
