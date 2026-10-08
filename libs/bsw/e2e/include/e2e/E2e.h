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
 * \ingroup e2e
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <etl/delegate.h>
#include <etl/map.h>
#include <etl/string.h>
#include <etl/vector.h>

namespace e2e
{

static constexpr size_t MAX_PROFILES       = 16U;
static constexpr size_t MAX_DATA_ID_LIST   = 16U;
static constexpr size_t MAX_SIGNAL_NAME_LEN = 31U;

enum class ProfileType : uint8_t
{
    P01,
    P02,
    P04,
    P05,
    P11
};

enum class CheckResult : uint8_t
{
    Ok,
    CrcError,
    CounterError,
    DataIdError,
    TimeoutError,
    ConfigError
};

/// Static E2E profile descriptor (normally generated from YAML -> blob).
struct ProfileConfig
{
    uint8_t profileId{0U};
    ProfileType type{ProfileType::P01};
    etl::string<MAX_SIGNAL_NAME_LEN> signalName{};
    uint32_t pduId{0U};
    uint32_t dataId{0U};
    etl::vector<uint8_t, MAX_DATA_ID_LIST> dataIdList{};
    uint8_t counterSize{4U}; // bits: 4, 8, 16
    uint32_t timeoutMs{100U};
};

using TickProvider = etl::delegate<uint32_t()>;

/// End-to-end protection library (AUTOSAR-style profiles P01/P02/P04/P05/P11).
class E2e
{
public:
    static E2e& instance();

    E2e(E2e const&)            = delete;
    E2e& operator=(E2e const&) = delete;

    bool init(ProfileConfig const* profiles, size_t count);
    bool isInitialized() const { return _initialized; }

    CheckResult protect(uint8_t profileId, uint8_t* data, uint16_t length, bool increment = true);
    CheckResult check(uint8_t profileId, uint8_t const* data, uint16_t length);

    CheckResult protectProfile1(uint8_t* data, uint16_t len, uint32_t dataId, bool inc = true);
    CheckResult checkProfile1(uint8_t const* data, uint16_t len, uint32_t dataId);
    CheckResult protectProfile2(
        uint8_t* data, uint16_t len, uint8_t const* list, size_t listLen, bool inc = true);
    CheckResult checkProfile2(
        uint8_t const* data, uint16_t len, uint8_t const* list, size_t listLen);
    CheckResult protectProfile4(uint8_t* data, uint16_t len, uint32_t dataId, bool inc = true);
    CheckResult checkProfile4(uint8_t const* data, uint16_t len, uint32_t dataId);
    CheckResult protectProfile5(uint8_t* data, uint16_t len, uint32_t dataId, bool inc = true);
    CheckResult checkProfile5(uint8_t const* data, uint16_t len, uint32_t dataId);
    CheckResult protectProfile11(uint8_t* data, uint16_t len, uint32_t dataId, bool inc = true);
    CheckResult checkProfile11(uint8_t const* data, uint16_t len, uint32_t dataId);

    uint32_t getCounter(uint8_t profileId) const;
    void reset(uint8_t profileId);
    void setTickProvider(TickProvider provider) { _tickProvider = provider; }
    void mainFunction();

private:
    E2e() = default;

    struct State
    {
        ProfileConfig config{};
        uint32_t txCounter{0U};
        uint32_t rxCounter{0U};
        bool rxSynced{false};
        uint32_t lastRxTick{0U};
    };

    State* find(uint8_t profileId);
    State const* find(uint8_t profileId) const;
    uint32_t now() const;
    static uint32_t maskFor(uint8_t bits);
    static bool validateCounter(uint32_t received, uint32_t expected, uint8_t bits, bool synced);

    etl::map<uint8_t, State, MAX_PROFILES> _states{};
    bool _initialized{false};
    TickProvider _tickProvider{};
    uint32_t _tick{0U};
};

} // namespace e2e
