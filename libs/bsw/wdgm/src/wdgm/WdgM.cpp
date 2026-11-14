/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <wdgm/WdgM.h>

namespace wdgm
{

WdgM& WdgM::instance()
{
    static WdgM inst{};
    return inst;
}

bool WdgM::init(EntityConfig const* entities, size_t count)
{
    _entities.clear();
    _qaVerifiers.clear();
    _initialized     = false;
    _lastCycleFailed = false;
    _tick            = 0U;
    _challengeCounter = 0U;

    if ((entities == nullptr) || (count == 0U) || (count > MAX_ENTITIES))
    {
        return false;
    }
    for (size_t i = 0U; i < count; ++i)
    {
        if (!registerEntity(entities[i].id, entities[i]))
        {
            return false;
        }
    }
    _initialized = true;
    return true;
}

bool WdgM::registerEntity(uint32_t id, EntityConfig const& cfgIn)
{
    if (_entities.size() >= MAX_ENTITIES)
    {
        return false;
    }
    if (_entities.find(id) != _entities.end())
    {
        return false;
    }
    EntityConfig cfg = cfgIn;
    // Normalize compatibility aliases into the primary fields.
    if (cfg.minAlive > 0U) { cfg.aliveMin = cfg.minAlive; }
    if (cfg.maxAlive > 0U) { cfg.aliveMax = cfg.maxAlive; }
    if (cfg.expectedAlive > 0U)
    {
        cfg.aliveMin = cfg.expectedAlive;
        cfg.aliveMax = cfg.expectedAlive;
    }
    if (cfg.aliveTolerance > 0U)
    {
        if (cfg.aliveMin > cfg.aliveTolerance) { cfg.aliveMin -= cfg.aliveTolerance; }
        else { cfg.aliveMin = 0U; }
        cfg.aliveMax += cfg.aliveTolerance;
    }
    if (cfg.windowMinMs == 0U)
    {
        if (cfg.windowMin > 0U) { cfg.windowMinMs = cfg.windowMin; }
        if (cfg.minWindowMs > 0U) { cfg.windowMinMs = cfg.minWindowMs; }
    }
    if (cfg.windowMaxMs == 0U)
    {
        if (cfg.windowMax > 0U) { cfg.windowMaxMs = cfg.windowMax; }
        if (cfg.maxWindowMs > 0U) { cfg.windowMaxMs = cfg.maxWindowMs; }
    }
    if (cfg.deadlineMaxMs == 0U && cfg.deadlineMax > 0U)
    {
        cfg.deadlineMaxMs = cfg.deadlineMax;
    }
    cfg.id           = id;
    Entity e{};
    e.config         = cfg;
    e.qaSatisfied    = false;
    _entities[id]    = e;
    return true;
}

void WdgM::setQaVerifier(uint32_t entityId, QaVerifier verifier)
{
    _qaVerifiers[entityId] = verifier;
}

WdgM::Entity* WdgM::find(uint32_t id)
{
    auto it = _entities.find(id);
    if (it == _entities.end())
    {
        return nullptr;
    }
    return &it->second;
}

WdgM::Entity const* WdgM::find(uint32_t id) const
{
    auto it = _entities.find(id);
    if (it == _entities.end())
    {
        return nullptr;
    }
    return &it->second;
}

uint32_t WdgM::now() const
{
    if (_tickProvider)
    {
        return _tickProvider();
    }
    return _tick;
}

void WdgM::checkpointReached(uint32_t entityId, uint32_t /*checkpointId*/)
{
    Entity* e = find(entityId);
    if (e == nullptr)
    {
        return;
    }
    ++e->aliveCount;

    uint32_t const t = now();
    if (e->hasLastTick)
    {
        uint32_t const dt = t - e->lastTick;
        if ((e->config.windowMinMs > 0U) && (dt < e->config.windowMinMs))
        {
            e->windowViolation = true;
        }
        if ((e->config.windowMaxMs > 0U) && (dt > e->config.windowMaxMs))
        {
            e->windowViolation = true;
        }
    }
    e->lastTick    = t;
    e->hasLastTick = true;
}

uint32_t WdgM::getChallenge(uint32_t entityId)
{
    Entity* e = find(entityId);
    if (e == nullptr)
    {
        return 0U;
    }
    ++_challengeCounter;
    e->currentChallenge = _challengeCounter;
    e->qaPending        = true;
    e->qaSatisfied      = false;
    return e->currentChallenge;
}

bool WdgM::checkResponse(uint32_t entityId, uint32_t response)
{
    Entity* e = find(entityId);
    if (e == nullptr)
    {
        return false;
    }
    bool ok = false;
    auto it = _qaVerifiers.find(entityId);
    if (it != _qaVerifiers.end() && it->second)
    {
        ok = it->second(e->currentChallenge, response);
    }
    else
    {
        // Default verifier: response must echo the challenge (optionally XOR key).
        uint32_t const expected = e->currentChallenge ^ e->config.qaKey;
        ok = (response == expected);
    }
    e->qaSatisfied = ok;
    e->qaPending   = false;
    return ok;
}

void WdgM::mainFunction()
{
    ++_tick;
    bool anyFail = false;
    for (auto& kv : _entities)
    {
        Entity& e = kv.second;
        bool fail = false;

        if (e.aliveCount < e.config.aliveMin || e.aliveCount > e.config.aliveMax)
        {
            fail = true;
        }
        if (e.windowViolation)
        {
            fail = true;
        }
        if (e.config.qaEnabled && !e.qaSatisfied)
        {
            fail = true;
        }
        if (e.config.deadlineMaxMs > 0U && e.hasLastTick)
        {
            uint32_t const dt = now() - e.lastTick;
            if (dt > e.config.deadlineMaxMs)
            {
                fail = true;
            }
        }

        e.aliveCount       = 0U;
        e.windowViolation  = false;
        e.qaSatisfied      = false;
        e.qaPending        = false;
        e.failedLastCycle  = fail;
        if (fail)
        {
            anyFail = true;
        }
    }
    _lastCycleFailed = anyFail;
    if (anyFail && _safeStateCallback)
    {
        _safeStateCallback();
    }
}

bool WdgM::isEntityFailed(uint32_t entityId) const
{
    Entity const* e = find(entityId);
    if (e == nullptr)
    {
        return false;
    }
    return e->failedLastCycle;
}

} // namespace wdgm