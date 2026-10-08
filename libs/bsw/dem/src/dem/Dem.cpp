/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <dem/Dem.h>

namespace dem
{

Dem& Dem::instance()
{
    static Dem inst{};
    return inst;
}

bool Dem::init(DtcConfig const* dtcs, size_t count, DemConfig const& cfg)
{
    _entries.clear();
    _cfg          = cfg;
    _initialized  = false;
    _suppressed   = false;
    _liveSignals.clear();
    if ((dtcs == nullptr) || (count == 0U) || (count > MAX_DTCS))
    {
        return false;
    }
    for (size_t i = 0U; i < count; ++i)
    {
        Entry e{};
        e.config     = dtcs[i];
        e.configured = true;
        e.status     = STATUS_NOT_COMPLETED_SINCE_CLEAR | STATUS_NOT_COMPLETED_THIS_OC;
        e.ext.dtc    = e.config.dtc;
        e.freeze.dtc = e.config.dtc;
        _entries[e.config.dtc] = e;
    }
    _initialized = true;
    return true;
}

Dem::Entry* Dem::find(uint32_t dtc)
{
    auto it = _entries.find(dtc);
    if (it == _entries.end())
    {
        return nullptr;
    }
    return &it->second;
}

Dem::Entry const* Dem::find(uint32_t dtc) const
{
    auto it = _entries.find(dtc);
    if (it == _entries.end())
    {
        return nullptr;
    }
    return &it->second;
}

uint32_t Dem::now() const
{
    if (_timeProvider)
    {
        return _timeProvider();
    }
    return _tick;
}

void Dem::storeFreezeFrame(Entry& e)
{
    if (!e.config.freezeFrameEnabled)
    {
        return;
    }
    e.freeze.dtc       = e.config.dtc;
    e.freeze.timestamp = now();
    e.freeze.signals.clear();
    for (auto const& s : _liveSignals)
    {
        if (e.freeze.signals.size() >= e.freeze.signals.max_size())
        {
            break;
        }
        e.freeze.signals.push_back(s);
    }
}

void Dem::recordFault(uint32_t dtc, Severity severity)
{
    recordFaultWithSignals(dtc, severity, nullptr, 0U);
}

void Dem::recordFaultWithSignals(
    uint32_t dtc, Severity severity, FreezeSignal const* signals, size_t signalCount)
{
    if (!_initialized || _suppressed)
    {
        return;
    }
    Entry* e = find(dtc);
    if (e == nullptr)
    {
        return;
    }
    (void)severity;
    bool const firstTime = ((e->status & STATUS_CONFIRMED) == 0U);
    e->status &= static_cast<uint8_t>(~(STATUS_NOT_COMPLETED_SINCE_CLEAR | STATUS_NOT_COMPLETED_THIS_OC));
    e->status = static_cast<uint8_t>(e->status | STATUS_TEST_FAILED | STATUS_TEST_FAILED_THIS_OC
                                     | STATUS_FAILED_SINCE_CLEAR);
    if (e->config.pendingEnabled)
    {
        e->status = static_cast<uint8_t>(e->status | STATUS_PENDING);
    }
    // Confirm immediately (single-trip confirmation subset); occurrence always counts.
    e->status = static_cast<uint8_t>(e->status | STATUS_CONFIRMED);
    if (e->config.obdClass != ObdClass::ClassA)
    {
        e->status = static_cast<uint8_t>(e->status | STATUS_WARNING_INDICATOR);
    }
    if (e->occurrenceCounter < 0xFFFFU)
    {
        ++e->occurrenceCounter;
    }
    e->lastFailureTime = now();
    if (firstTime)
    {
        e->firstFailureTime = e->lastFailureTime;
    }
    e->agingCounter = 0U;
    if ((signals != nullptr) && (signalCount > 0U))
    {
        e->freeze.dtc       = dtc;
        e->freeze.timestamp = now();
        e->freeze.signals.clear();
        for (size_t i = 0U; (i < signalCount) && (e->freeze.signals.size() < e->freeze.signals.max_size());
             ++i)
        {
            e->freeze.signals.push_back(signals[i]);
        }
    }
    else
    {
        storeFreezeFrame(*e);
    }
    enforceMemoryLimits();
}

void Dem::clearFault(uint32_t dtc)
{
    Entry* e = find(dtc);
    if ((e == nullptr) || !_initialized)
    {
        return;
    }
    // Fault condition gone: keep CONFIRMED/PENDING for aging, clear TEST_FAILED bits.
    e->status = static_cast<uint8_t>(e->status & ~(STATUS_TEST_FAILED | STATUS_TEST_FAILED_THIS_OC));
}

uint8_t Dem::getDtcStatus(uint32_t dtc) const
{
    Entry const* e = find(dtc);
    if (e == nullptr)
    {
        return 0U;
    }
    return e->status;
}

etl::vector<uint32_t, MAX_DTCS> Dem::getStoredDtcs() const
{
    etl::vector<uint32_t, MAX_DTCS> out{};
    for (auto const& kv : _entries)
    {
        if (((kv.second.status & STATUS_CONFIRMED) != 0U)
            || ((kv.second.status & STATUS_PENDING) != 0U))
        {
            out.push_back(kv.first);
        }
    }
    return out;
}

etl::vector<DtcWithStatus, MAX_DTCS> Dem::getDtcsWithStatus() const
{
    etl::vector<DtcWithStatus, MAX_DTCS> out{};
    for (auto const& kv : _entries)
    {
        Entry const& e = kv.second;
        if (((e.status & STATUS_CONFIRMED) != 0U) || ((e.status & STATUS_PENDING) != 0U)
            || ((e.status & STATUS_FAILED_SINCE_CLEAR) != 0U))
        {
            DtcWithStatus ws{};
            ws.dtc                = kv.first;
            ws.status             = e.status;
            ws.occurrenceCounter  = e.occurrenceCounter;
            out.push_back(ws);
        }
    }
    return out;
}

bool Dem::getFreezeFrame(uint32_t dtc, FreezeFrame& out) const
{
    Entry const* e = find(dtc);
    if (e == nullptr)
    {
        return false;
    }
    out = e->freeze;
    return true;
}

bool Dem::getExtendedData(uint32_t dtc, ExtendedData& out) const
{
    Entry const* e = find(dtc);
    if (e == nullptr)
    {
        return false;
    }
    out = e->ext;
    return true;
}

bool Dem::storeExtendedData(uint32_t dtc, uint8_t const* data, uint8_t length)
{
    Entry* e = find(dtc);
    if ((e == nullptr) || (data == nullptr))
    {
        return false;
    }
    if ((length == 0U) || (length > e->config.extendedDataSize)
        || (length > e->ext.data.max_size()))
    {
        return false;
    }
    e->ext.dtc = dtc;
    e->ext.data.clear();
    for (uint8_t i = 0U; i < length; ++i)
    {
        e->ext.data.push_back(data[i]);
    }
    return true;
}

void Dem::clearAllDtcs()
{
    for (auto& kv : _entries)
    {
        Entry& e = kv.second;
        if (e.config.permanent)
        {
            continue; // WWH-OBD: permanent DTCs survive a global clear.
        }
        e.status = STATUS_NOT_COMPLETED_SINCE_CLEAR | STATUS_NOT_COMPLETED_THIS_OC;
        e.occurrenceCounter = 0U;
        e.agingCounter      = 0U;
        e.freeze.signals.clear();
        e.ext.data.clear();
    }
}

void Dem::notifyOperationCycle(char const* cycleName)
{
    (void)cycleName;
    if (!_initialized)
    {
        return;
    }
    ++_tick;
    for (auto& kv : _entries)
    {
        Entry& e = kv.second;
        if (!e.config.agingEnabled || e.config.permanent)
        {
            continue;
        }
        bool const failedNow = ((e.status & STATUS_TEST_FAILED) != 0U);
        if (failedNow)
        {
            e.agingCounter = 0U;
            continue;
        }
        if (((e.status & STATUS_CONFIRMED) != 0U) || ((e.status & STATUS_PENDING) != 0U))
        {
            if (e.agingCounter < 0xFFU)
            {
                ++e.agingCounter;
            }
            if (e.agingCounter >= e.config.agingCycles)
            {
                e.status = STATUS_NOT_COMPLETED_SINCE_CLEAR | STATUS_NOT_COMPLETED_THIS_OC;
                e.occurrenceCounter = 0U;
                e.agingCounter      = 0U;
            }
        }
    }
}

void Dem::setFreezeSignals(FreezeSignal const* signals, size_t count)
{
    _liveSignals.clear();
    if (signals == nullptr)
    {
        return;
    }
    for (size_t i = 0U; (i < count) && (_liveSignals.size() < _liveSignals.max_size()); ++i)
    {
        _liveSignals.push_back(signals[i]);
    }
}

bool Dem::isMinor(Entry const& e) { return e.config.severity == Severity::Minor; }

void Dem::enforceMemoryLimits()
{
    // Count confirmed/critical-major (primary) vs minor (secondary); drop lowest-priority
    // minor entries beyond the secondary limit by resetting them to never-failed.
    size_t minorActive = 0U;
    for (auto const& kv : _entries)
    {
        if (isMinor(kv.second) && (((kv.second.status & STATUS_CONFIRMED) != 0U)))
        {
            ++minorActive;
        }
    }
    if (minorActive <= _cfg.maxSecondaryDtcs)
    {
        return;
    }
    for (auto& kv : _entries)
    {
        if (minorActive <= _cfg.maxSecondaryDtcs)
        {
            break;
        }
        if (isMinor(kv.second) && (((kv.second.status & STATUS_CONFIRMED) != 0U)))
        {
            kv.second.status = STATUS_NOT_COMPLETED_SINCE_CLEAR | STATUS_NOT_COMPLETED_THIS_OC;
            --minorActive;
        }
    }
}

size_t Dem::serialize(etl::span<uint8_t> out) const
{
    // Layout: [count:1][entries: dtc:4 status:1 occ:2]*  (only stored DTCs)
    auto stored = getDtcsWithStatus();
    size_t const need = 1U + (stored.size() * 7U);
    if (out.size() < need)
    {
        return 0U;
    }
    out[0U]      = static_cast<uint8_t>(stored.size());
    size_t pos   = 1U;
    for (auto const& w : stored)
    {
        out[pos++] = static_cast<uint8_t>((w.dtc >> 24U) & 0xFFU);
        out[pos++] = static_cast<uint8_t>((w.dtc >> 16U) & 0xFFU);
        out[pos++] = static_cast<uint8_t>((w.dtc >> 8U) & 0xFFU);
        out[pos++] = static_cast<uint8_t>(w.dtc & 0xFFU);
        out[pos++] = w.status;
        out[pos++] = static_cast<uint8_t>((w.occurrenceCounter >> 8U) & 0xFFU);
        out[pos++] = static_cast<uint8_t>(w.occurrenceCounter & 0xFFU);
    }
    return need;
}

bool Dem::deserialize(etl::span<uint8_t const> in)
{
    if (in.empty())
    {
        return false;
    }
    size_t const count = in[0U];
    if ((1U + (count * 7U)) > in.size())
    {
        return false;
    }
    size_t pos = 1U;
    for (size_t i = 0U; i < count; ++i)
    {
        uint32_t const dtc = (static_cast<uint32_t>(in[pos]) << 24U)
            | (static_cast<uint32_t>(in[pos + 1U]) << 16U)
            | (static_cast<uint32_t>(in[pos + 2U]) << 8U) | in[pos + 3U];
        uint8_t const status = in[pos + 4U];
        auto occ = static_cast<uint16_t>(
            (static_cast<uint16_t>(in[pos + 5U]) << 8U) | in[pos + 6U]);
        pos += 7U;
        Entry* e = find(dtc);
        if (e != nullptr)
        {
            e->status             = status;
            e->occurrenceCounter  = occ;
        }
    }
    return true;
}

void Dem::mainFunction()
{
    ++_tick;
    // Timeout/aging tick hook: real aging happens on operation-cycle notifications.
}

} // namespace dem
