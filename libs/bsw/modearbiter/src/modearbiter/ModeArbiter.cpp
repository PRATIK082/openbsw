/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <modearbiter/ModeArbiter.h>

#include <algorithm>
#include <map>
#include <string>

namespace modearbiter
{

namespace
{

struct Rule
{
    std::string name;
    ModeArbiter::RuleCondition condition;
    bool hasMode = false;
    EcuMode mode = EcuMode::UNINIT;
    std::vector<std::string> actions;
};

std::vector<Rule>& rules()
{
    static std::vector<Rule> r;
    return r;
}

std::map<std::string, bool>& condValues()
{
    static std::map<std::string, bool> v;
    return v;
}

std::map<std::string, ModeArbiter::RuleCondition>& condFns()
{
    static std::map<std::string, ModeArbiter::RuleCondition> f;
    return f;
}

std::map<std::string, ModeArbiter::ActionHandler>& actionHandlers()
{
    static std::map<std::string, ModeArbiter::ActionHandler> h;
    return h;
}

bool conditionValue(std::string const& name)
{
    auto fit = condFns().find(name);
    if (fit != condFns().end() && fit->second)
    {
        return fit->second();
    }
    auto vit = condValues().find(name);
    if (vit != condValues().end())
    {
        return vit->second;
    }
    return false;
}

} // namespace

ModeArbiter& ModeArbiter::getInstance()
{
    static ModeArbiter instance;
    return instance;
}

::ecumode::CommStateManagerPhase5& ModeArbiter::csm() const
{
    return ::ecumode::CommStateManagerPhase5::getInstance();
}

bool ModeArbiter::init()
{
    clear();
    return true;
}

void ModeArbiter::shutdown()
{
    clear();
}

void ModeArbiter::clear()
{
    csm().clear();
    rules().clear();
    condValues().clear();
    condFns().clear();
    actionHandlers().clear();
}

bool ModeArbiter::requestMode(EcuMode mode)
{
    csm().requestEcuMode(mode);
    return true;
}

EcuMode ModeArbiter::getCurrentMode() const
{
    return csm().getCurrentEcuMode();
}

void ModeArbiter::setDefaultPolicy(EcuMode mode, FrameFilterPolicy policy)
{
    csm().setDefaultPolicy(mode, policy);
}

void ModeArbiter::addException(EcuMode mode, uint32_t frameId, uint8_t channelType, FrameFilterPolicy policy)
{
    csm().addException(mode, frameId, channelType, policy);
}

void ModeArbiter::clearPolicies()
{
    csm().clearPolicies();
}

FrameFilterPolicy ModeArbiter::getFrameFilterPolicy(uint32_t frameId, uint8_t channelType) const
{
    return csm().getFrameFilterPolicy(frameId, channelType);
}

bool ModeArbiter::isFrameAllowed(uint32_t frameId, uint8_t channelType, bool isTx) const
{
    return csm().isFrameAllowed(frameId, channelType, isTx);
}

uint32_t ModeArbiter::getBlockCount() const
{
    return csm().getBlockCount();
}

void ModeArbiter::registerActionHandler(std::string const& name, ActionHandler handler)
{
    csm().registerActionHandler(name, handler);
    actionHandlers()[name] = handler;
}

void ModeArbiter::setModeActions(EcuMode mode, std::vector<std::string> const& actions)
{
    csm().setModeActions(mode, actions);
}

void ModeArbiter::setModeChangeCallback(ModeChangeCallback cb)
{
    csm().setModeChangeCallback(cb);
}

void ModeArbiter::registerWakeupSource(uint32_t id, WakeupChecker checker)
{
    csm().registerWakeupSource(id, checker);
}

void ModeArbiter::unregisterWakeupSource(uint32_t id)
{
    csm().unregisterWakeupSource(id);
}

bool ModeArbiter::isWakeupSourceActive(uint32_t id) const
{
    return csm().isWakeupSourceActive(id);
}

std::vector<uint32_t> ModeArbiter::getActiveWakeupSources() const
{
    return csm().getActiveWakeupSources();
}

bool ModeArbiter::hasActiveWakeup() const
{
    return csm().hasActiveWakeup();
}

uint32_t ModeArbiter::getWakeupReason() const
{
    return csm().getWakeupReason();
}

void ModeArbiter::processWakeupSources()
{
    csm().processWakeupSources();
}

void ModeArbiter::requestRun(std::string const& client)
{
    csm().requestRun(client);
}

void ModeArbiter::releaseRun(std::string const& client)
{
    csm().releaseRun(client);
}

bool ModeArbiter::hasRunRequests() const
{
    return csm().hasRunRequests();
}

size_t ModeArbiter::getRunRequestCount() const
{
    return csm().getRunRequestCount();
}

void ModeArbiter::setModeCommunication(EcuMode mode, bool enabled)
{
    csm().setModeCommunication(mode, enabled);
}

bool ModeArbiter::isModeCommunicationEnabled(EcuMode mode) const
{
    return csm().isModeCommunicationEnabled(mode);
}

bool ModeArbiter::loadConfig(std::string const& jsonText, std::string& error)
{
    return csm().loadConfig(jsonText, error);
}

void ModeArbiter::mainFunction(uint32_t nowMs)
{
    evaluateRules();
    csm().mainFunction(nowMs);
}

void ModeArbiter::setCondition(std::string const& name, bool value)
{
    condValues()[name] = value;
    condFns().erase(name);
}

bool ModeArbiter::getCondition(std::string const& name) const
{
    return conditionValue(name);
}

void ModeArbiter::registerCondition(std::string const& name, RuleCondition condition)
{
    condFns()[name] = condition;
    condValues().erase(name);
}

void ModeArbiter::addRule(std::string const& name, RuleCondition condition, EcuMode mode)
{
    auto& rs = rules();
    rs.erase(std::remove_if(rs.begin(), rs.end(),
                            [&name](Rule const& r) { return r.name == name; }),
             rs.end());
    Rule r;
    r.name      = name;
    r.condition = condition;
    r.hasMode   = true;
    r.mode      = mode;
    rs.push_back(r);
}

void ModeArbiter::addRule(std::string const& name, std::string const& conditionName, EcuMode mode)
{
    std::string cn = conditionName;
    addRule(name, [cn]() { return conditionValue(cn); }, mode);
}

void ModeArbiter::addRule(std::string const& name, RuleCondition condition, std::vector<std::string> const& actions)
{
    auto& rs = rules();
    rs.erase(std::remove_if(rs.begin(), rs.end(),
                            [&name](Rule const& r) { return r.name == name; }),
             rs.end());
    Rule r;
    r.name      = name;
    r.condition = condition;
    r.hasMode   = false;
    r.actions   = actions;
    rs.push_back(r);
}

void ModeArbiter::addRule(std::string const& name, std::string const& conditionName, std::vector<std::string> const& actions)
{
    std::string cn = conditionName;
    addRule(name, [cn]() { return conditionValue(cn); }, actions);
}

void ModeArbiter::addRule(std::string const& name, RuleCondition condition, EcuMode mode, std::vector<std::string> const& actions)
{
    auto& rs = rules();
    rs.erase(std::remove_if(rs.begin(), rs.end(),
                            [&name](Rule const& r) { return r.name == name; }),
             rs.end());
    Rule r;
    r.name      = name;
    r.condition = condition;
    r.hasMode   = true;
    r.mode      = mode;
    r.actions   = actions;
    rs.push_back(r);
}

void ModeArbiter::addRule(std::string const& name, std::string const& conditionName, EcuMode mode, std::vector<std::string> const& actions)
{
    std::string cn = conditionName;
    addRule(name, [cn]() { return conditionValue(cn); }, mode, actions);
}

void ModeArbiter::removeRule(std::string const& name)
{
    auto& rs = rules();
    rs.erase(std::remove_if(rs.begin(), rs.end(),
                            [&name](Rule const& r) { return r.name == name; }),
             rs.end());
}

bool ModeArbiter::evaluateRules()
{
    for (auto const& r : rules())
    {
        if (r.condition && r.condition())
        {
            if (r.hasMode)
            {
                csm().requestEcuMode(r.mode);
            }
            for (auto const& actionName : r.actions)
            {
                auto hit = actionHandlers().find(actionName);
                if (hit != actionHandlers().end() && hit->second)
                {
                    hit->second(r.hasMode ? r.mode : csm().getCurrentEcuMode());
                }
            }
        }
    }
    return true;
}

size_t ModeArbiter::getRuleCount() const
{
    return rules().size();
}

std::vector<std::string> ModeArbiter::getRuleNames() const
{
    std::vector<std::string> names;
    for (auto const& r : rules())
    {
        names.push_back(r.name);
    }
    return names;
}

} // namespace modearbiter
