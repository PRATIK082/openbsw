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
 * \ingroup wdgm
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <etl/map.h>

namespace wdgm
{

static constexpr size_t MAX_ENTITIES = 32U;

/// Supervised entity configuration (window + alive + Q&A).
struct EntityConfig
{
    uint32_t id{0U};
    /// Minimum alive indications accepted per supervision cycle.
    uint32_t aliveMin{1U};
    /// Maximum alive indications accepted per supervision cycle.
    uint32_t aliveMax{1U};
    /// Minimum time (ms) between consecutive checkpoints (0 = disabled).
    uint32_t windowMinMs{0U};
    /// Maximum time (ms) between consecutive checkpoints (0 = disabled).
    uint32_t windowMaxMs{0U};
    /// Maximum time (ms) since last checkpoint before deadline violation (0 = disabled).
    uint32_t deadlineMaxMs{0U};
    /// Enable question/answer supervision for this entity.
    bool qaEnabled{false};
    /// Opaque key used by the default Q&A verifier.
    uint32_t qaKey{0U};

    // Compatibility aliases (0 = unspecified, falls back to primary fields).
    uint32_t minAlive{0U};
    uint32_t maxAlive{0U};
    uint32_t expectedAlive{0U};
    uint32_t aliveTolerance{0U};
    uint32_t windowMin{0U};
    uint32_t windowMax{0U};
    uint32_t deadlineMax{0U};
    uint32_t minWindowMs{0U};
    uint32_t maxWindowMs{0U};
};

using TickProvider     = std::function<uint32_t()>;
using SafeStateCallback = std::function<void()>;
/// Verifier: returns true if \p response matches \p challenge.
using QaVerifier      = std::function<bool(uint32_t, uint32_t)>;

/// Compatibility aliases.
using SupervisedEntityConfig = EntityConfig;
using Config                 = EntityConfig;

/**
 * Watchdog Manager: software supervision (alive + window/deadline + Q&A).
 *
 * No hardware watchdog is involved; on supervision failure the registered
 * safe-state callback is invoked from mainFunction().
 *
 * \req SYS-SAFETY-001 Supervised entity registration and alive counters.
 * \req SYS-SAFETY-002 Alive supervision (expected count within configured window).
 * \req SYS-SAFETY-003 Window/deadline supervision of checkpoints.
 * \req SYS-SAFETY-004 Question/answer (challenge/response) supervision.
 * \req SYS-SAFETY-005 Safe-state callback invocation on supervision failure.
 * \trace TEST: WdgMTest (libs/bsw/wdgm/test)
 */
class WdgM
{
public:
    static WdgM& instance();
    static WdgM& getInstance() { return instance(); }

    WdgM(WdgM const&)            = delete;
    WdgM& operator=(WdgM const&) = delete;

    bool init(EntityConfig const* entities, size_t count);
    bool isInitialized() const { return _initialized; }

    bool registerEntity(uint32_t id, EntityConfig const& cfg);
    bool registerSupervisedEntity(uint32_t id, EntityConfig const& cfg) { return registerEntity(id, cfg); }
    bool addSupervisedEntity(uint32_t id, EntityConfig const& cfg) { return registerEntity(id, cfg); }

    void setTickProvider(TickProvider provider) { _tickProvider = provider; }
    void setSafeStateCallback(SafeStateCallback cb) { _safeStateCallback = cb; }
    void registerSafeStateCallback(SafeStateCallback cb) { setSafeStateCallback(cb); }
    void setQaVerifier(uint32_t entityId, QaVerifier verifier);

    /// Alive indication for an entity (optionally tagged with a checkpoint id).
    void checkpointReached(uint32_t entityId, uint32_t checkpointId = 0U);
    void aliveIndication(uint32_t entityId, uint32_t checkpointId = 0U) { checkpointReached(entityId, checkpointId); }
    void aliveCounterIncrement(uint32_t entityId) { checkpointReached(entityId, 0U); }

    /// Issue a Q&A challenge for an entity.
    uint32_t getChallenge(uint32_t entityId);
    uint32_t requestChallenge(uint32_t entityId) { return getChallenge(entityId); }
    /// Verify a Q&A response; returns true on match.
    bool checkResponse(uint32_t entityId, uint32_t response);
    bool verifyResponse(uint32_t entityId, uint32_t response) { return checkResponse(entityId, response); }
    bool qaCheck(uint32_t entityId, uint32_t response) { return checkResponse(entityId, response); }

    /// Evaluate all entities; invokes safe-state callback on any failure.
    void mainFunction();

    bool hasFailure() const { return _lastCycleFailed; }
    bool isFailed() const { return _lastCycleFailed; }
    bool anyFailure() const { return _lastCycleFailed; }
    bool isEntityFailed(uint32_t entityId) const;

private:
    WdgM() = default;

    struct Entity
    {
        EntityConfig config{};
        uint32_t aliveCount{0U};
        uint32_t lastTick{0U};
        bool hasLastTick{false};
        bool windowViolation{false};
        bool qaSatisfied{false};
        bool failedLastCycle{false};
        uint32_t currentChallenge{0U};
        bool qaPending{false};
    };

    Entity* find(uint32_t id);
    Entity const* find(uint32_t id) const;
    uint32_t now() const;

    etl::map<uint32_t, Entity, MAX_ENTITIES> _entities{};
    bool _initialized{false};
    TickProvider _tickProvider{};
    uint32_t _tick{0U};
    SafeStateCallback _safeStateCallback{};
    uint32_t _challengeCounter{0U};
    etl::map<uint32_t, QaVerifier, MAX_ENTITIES> _qaVerifiers{};
    bool _lastCycleFailed{false};
};

} // namespace wdgm