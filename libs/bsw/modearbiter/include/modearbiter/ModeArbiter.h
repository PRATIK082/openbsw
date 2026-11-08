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
 * \ingroup modearbiter
 *
 * ModeArbiter - BSW mode manager (alternative naming for BswM).
 *
 * Arbitrates ECU modes: evaluates rules (conditions) and on mode change
 * executes registered actions. Reuses ecumode::EcuMode and
 * ecumode::CommStateManagerPhase5 (no duplication).
 */
#pragma once

#include <ecumode/EcuMode.h>
#include <ecumode/CommStateManagerPhase5.h>

#include <commgateway/CommStateManager.h>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace modearbiter
{

using EcuMode           = ::ecumode::EcuMode;
using FrameFilterPolicy = ::ecumode::FrameFilterPolicy;

/// Parses an ECU mode name; delegates to ecumode.
inline bool parseEcuMode(std::string const& text, EcuMode& mode)
{
    return ::ecumode::parseEcuMode(text, mode);
}

/// Parses a frame filter policy name; delegates to ecumode.
inline bool parseFrameFilterPolicy(std::string const& text, FrameFilterPolicy& policy)
{
    return ::ecumode::parseFrameFilterPolicy(text, policy);
}

class ModeArbiter
{
public:
    static ModeArbiter& getInstance();
    static ModeArbiter& instance() { return getInstance(); }

    ModeArbiter() = default;
    ~ModeArbiter() = default;

    ModeArbiter(ModeArbiter const&) = delete;
    ModeArbiter& operator=(ModeArbiter const&) = delete;

    // ---- lifecycle ----
    bool init();
    void shutdown();
    void clear();

    bool isInitialised() const { return true; }

    // ---- mode request API ----
    bool requestMode(EcuMode mode);
    bool requestEcuMode(EcuMode mode) { return requestMode(mode); }
    bool request(EcuMode mode) { return requestMode(mode); }

    EcuMode getCurrentMode() const;
    EcuMode getCurrentEcuMode() const { return getCurrentMode(); }
    EcuMode getMode() const { return getCurrentMode(); }
    EcuMode currentMode() const { return getCurrentMode(); }

    // ---- frame filter policies (delegated) ----
    void setDefaultPolicy(EcuMode mode, FrameFilterPolicy policy);
    void addException(EcuMode mode, uint32_t frameId, uint8_t channelType, FrameFilterPolicy policy);
    void clearPolicies();

    FrameFilterPolicy getFrameFilterPolicy(uint32_t frameId, uint8_t channelType) const;
    bool isFrameAllowed(uint32_t frameId, uint8_t channelType, bool isTx) const;

    uint32_t getBlockCount() const;

    // ---- action execution on mode change ----
    using ActionHandler = ::ecumode::CommStateManagerPhase5::ActionHandler;

    void registerActionHandler(std::string const& name, ActionHandler handler);
    void registerAction(std::string const& name, ActionHandler handler)
    {
        registerActionHandler(name, handler);
    }
    void setActionHandler(std::string const& name, ActionHandler handler)
    {
        registerActionHandler(name, handler);
    }
    void addActionHandler(std::string const& name, ActionHandler handler)
    {
        registerActionHandler(name, handler);
    }

    void setModeActions(EcuMode mode, std::vector<std::string> const& actions);
    void setActions(EcuMode mode, std::vector<std::string> const& actions)
    {
        setModeActions(mode, actions);
    }
    void setModeActionList(EcuMode mode, std::vector<std::string> const& actions)
    {
        setModeActions(mode, actions);
    }

    using ModeChangeCallback = ::ecumode::CommStateManagerPhase5::ModeChangeCallback;
    void setModeChangeCallback(ModeChangeCallback cb);
    void setCallback(ModeChangeCallback cb) { setModeChangeCallback(cb); }

    // ---- wakeup sources (delegated) ----
    using WakeupChecker = ::ecumode::CommStateManagerPhase5::WakeupChecker;
    void registerWakeupSource(uint32_t id, WakeupChecker checker);
    void unregisterWakeupSource(uint32_t id);
    bool isWakeupSourceActive(uint32_t id) const;
    std::vector<uint32_t> getActiveWakeupSources() const;
    bool hasActiveWakeup() const;
    uint32_t getWakeupReason() const;
    void processWakeupSources();

    // ---- run requests (delegated) ----
    void requestRun(std::string const& client);
    void releaseRun(std::string const& client);
    bool hasRunRequests() const;
    size_t getRunRequestCount() const;

    // ---- mode-dependent communication (delegated) ----
    void setModeCommunication(EcuMode mode, bool enabled);
    bool isModeCommunicationEnabled(EcuMode mode) const;

    // ---- config (delegated) ----
    bool loadConfig(std::string const& jsonText, std::string& error);

    // ---- periodic processing ----
    void mainFunction(uint32_t nowMs);

    // ---- rules / conditions (mode arbitration) ----
    using RuleCondition = std::function<bool()>;

    void setCondition(std::string const& name, bool value);
    bool getCondition(std::string const& name) const;
    void registerCondition(std::string const& name, RuleCondition condition);

    void addRule(std::string const& name, RuleCondition condition, EcuMode mode);
    void addRule(std::string const& name, std::string const& conditionName, EcuMode mode);
    void addRule(std::string const& name, RuleCondition condition, std::vector<std::string> const& actions);
    void addRule(std::string const& name, std::string const& conditionName, std::vector<std::string> const& actions);
    void addRule(std::string const& name, RuleCondition condition, EcuMode mode, std::vector<std::string> const& actions);
    void addRule(std::string const& name, std::string const& conditionName, EcuMode mode, std::vector<std::string> const& actions);
    void addRule(std::string const& name, EcuMode mode, RuleCondition condition)
    {
        addRule(name, condition, mode);
    }
    void addRule(EcuMode mode, RuleCondition condition, std::string const& name)
    {
        addRule(name, condition, mode);
    }
    void addRule(EcuMode mode, std::string const& name, RuleCondition condition)
    {
        addRule(name, condition, mode);
    }
    void addRule(RuleCondition condition, EcuMode mode, std::string const& name)
    {
        addRule(name, condition, mode);
    }
    void addRule(RuleCondition condition, std::string const& name, EcuMode mode)
    {
        addRule(name, condition, mode);
    }
    void registerRule(std::string const& name, RuleCondition condition, EcuMode mode)
    {
        addRule(name, condition, mode);
    }
    void registerRule(std::string const& name, std::string const& conditionName, EcuMode mode)
    {
        addRule(name, conditionName, mode);
    }
    void registerRule(std::string const& name, EcuMode mode, RuleCondition condition)
    {
        addRule(name, condition, mode);
    }
    void registerRule(std::string const& name, EcuMode mode, std::string const& conditionName)
    {
        addRule(name, conditionName, mode);
    }
    void setRule(std::string const& name, EcuMode mode, RuleCondition condition)
    {
        addRule(name, condition, mode);
    }
    void setRule(std::string const& name, EcuMode mode, std::string const& conditionName)
    {
        addRule(name, conditionName, mode);
    }
    void setRule(std::string const& name, RuleCondition condition, EcuMode mode)
    {
        addRule(name, condition, mode);
    }
    void setRule(std::string const& name, std::string const& conditionName, EcuMode mode)
    {
        addRule(name, conditionName, mode);
    }

    void removeRule(std::string const& name);

    bool evaluateRules();
    bool evaluate() { return evaluateRules(); }
    bool evaluateConditions() { return evaluateRules(); }
    bool runRules() { return evaluateRules(); }
    bool arbitrate() { return evaluateRules(); }

    size_t getRuleCount() const;
    std::vector<std::string> getRuleNames() const;

private:
    ::ecumode::CommStateManagerPhase5& csm() const;
};

// short alias
using ModeManager = ModeArbiter;

} // namespace modearbiter
