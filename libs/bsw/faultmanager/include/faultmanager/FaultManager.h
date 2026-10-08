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
 * \ingroup faultmanager
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <etl/delegate.h>
#include <etl/map.h>
#include <etl/span.h>
#include <etl/string.h>
#include <etl/vector.h>

namespace faultmanager
{

static constexpr size_t MAX_DTCS            = 64U;
static constexpr size_t MAX_FREEZE_SIGNALS  = 6U;
static constexpr size_t MAX_EXTENDED_DATA   = 32U;
static constexpr size_t MAX_SIGNAL_NAME_LEN = 23U;
static constexpr size_t MAX_CYCLE_NAME_LEN  = 23U;

/// ISO 14229-1 DTC status bits.
static constexpr uint8_t STATUS_TEST_FAILED                = 0x01U;
static constexpr uint8_t STATUS_TEST_FAILED_THIS_OC         = 0x02U;
static constexpr uint8_t STATUS_PENDING                    = 0x04U;
static constexpr uint8_t STATUS_CONFIRMED                   = 0x08U;
static constexpr uint8_t STATUS_NOT_COMPLETED_SINCE_CLEAR  = 0x10U;
static constexpr uint8_t STATUS_FAILED_SINCE_CLEAR          = 0x20U;
static constexpr uint8_t STATUS_NOT_COMPLETED_THIS_OC       = 0x40U;
static constexpr uint8_t STATUS_WARNING_INDICATOR           = 0x80U;

enum class Severity : uint8_t
{
    Minor,
    Major,
    Critical
};

enum class ObdClass : uint8_t
{
    ClassA,
    ClassB,
    ClassC,
    ClassD
};

enum class Format : uint8_t
{
    Obdii,
    Uds,
    Oem
};

/// Static DTC descriptor (normally generated from YAML -> blob).
struct DtcConfig
{
    uint32_t dtc{0U};
    Format format{Format::Obdii};
    Severity severity{Severity::Major};
    ObdClass obdClass{ObdClass::ClassC};
    bool freezeFrameEnabled{true};
    uint8_t extendedDataSize{0U};
    bool agingEnabled{true};
    uint8_t agingCycles{40U};
    bool pendingEnabled{true};
    bool permanent{false};
};

struct FaultManagerConfig
{
    uint8_t maxPrimaryDtcs{50U};
    uint8_t maxSecondaryDtcs{100U};
    uint8_t maxFreezeFrames{5U};
};

struct FreezeSignal
{
    etl::string<MAX_SIGNAL_NAME_LEN> name{};
    float value{0.0F};
};

struct FreezeFrame
{
    uint32_t dtc{0U};
    uint32_t timestamp{0U};
    etl::vector<FreezeSignal, MAX_FREEZE_SIGNALS> signals{};
};

struct ExtendedData
{
    uint32_t dtc{0U};
    etl::vector<uint8_t, MAX_EXTENDED_DATA> data{};
};

struct DtcWithStatus
{
    uint32_t dtc{0U};
    uint8_t status{0U};
    uint16_t occurrenceCounter{0U};
};

using TimeProvider = etl::delegate<uint32_t()>;

/// Diagnostic Event Manager: DTC storage, freeze frames, aging.
class FaultManager
{
public:
    static FaultManager& instance();

    FaultManager(FaultManager const&)            = delete;
    FaultManager& operator=(FaultManager const&) = delete;

    bool init(DtcConfig const* dtcs, size_t count, FaultManagerConfig const& cfg = FaultManagerConfig{});
    bool isInitialized() const { return _initialized; }

    void recordFault(uint32_t dtc, Severity severity = Severity::Major);
    void recordFaultWithSignals(
        uint32_t dtc, Severity severity, FreezeSignal const* signals, size_t signalCount);
    void clearFault(uint32_t dtc);

    uint8_t getDtcStatus(uint32_t dtc) const;
    etl::vector<uint32_t, MAX_DTCS> getStoredDtcs() const;
    etl::vector<DtcWithStatus, MAX_DTCS> getDtcsWithStatus() const;

    bool getFreezeFrame(uint32_t dtc, FreezeFrame& out) const;
    bool getExtendedData(uint32_t dtc, ExtendedData& out) const;
    bool storeExtendedData(uint32_t dtc, uint8_t const* data, uint8_t length);

    void clearAllDtcs();
    void notifyOperationCycle(char const* cycleName);
    void setDtcRecordingSuppressed(bool suppressed) { _suppressed = suppressed; }
    bool isDtcRecordingSuppressed() const { return _suppressed; }

    void setTimeProvider(TimeProvider provider) { _timeProvider = provider; }
    void setFreezeSignals(FreezeSignal const* signals, size_t count);

    /// Persistence persistence hooks (Phase-6): flat binary image of confirmed DTC entries.
    size_t serialize(etl::span<uint8_t> out) const;
    bool deserialize(etl::span<uint8_t const> in);

    void mainFunction();

private:
    FaultManager() = default;

    struct Entry
    {
        DtcConfig config{};
        uint8_t status{STATUS_NOT_COMPLETED_SINCE_CLEAR | STATUS_NOT_COMPLETED_THIS_OC};
        uint16_t occurrenceCounter{0U};
        uint32_t firstFailureTime{0U};
        uint32_t lastFailureTime{0U};
        uint8_t agingCounter{0U};
        bool configured{false};
        FreezeFrame freeze{};
        ExtendedData ext{};
    };

    Entry* find(uint32_t dtc);
    Entry const* find(uint32_t dtc) const;
    uint32_t now() const;
    void storeFreezeFrame(Entry& e);
    void enforceMemoryLimits();
    static bool isMinor(Entry const& e);

    etl::map<uint32_t, Entry, MAX_DTCS> _entries{};
    FaultManagerConfig _cfg{};
    bool _initialized{false};
    bool _suppressed{false};
    TimeProvider _timeProvider{};
    uint32_t _tick{0U};
    etl::vector<FreezeSignal, MAX_FREEZE_SIGNALS> _liveSignals{};
};

} // namespace faultmanager
