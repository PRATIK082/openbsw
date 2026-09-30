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
    void addException(EcuMode mode, uint32_t frameId, uint8_t channelType,
                      FrameFilterPolicy policy);
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
};

} // namespace ecumode
