/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <nvram/Fee.h>

namespace nvram
{

Fee::Fee(uint32_t numSectors, uint32_t sectorSize, uint32_t maxEraseCycles)
: _numSectors(numSectors), _sectorSize(sectorSize), _maxEraseCycles(maxEraseCycles)
{
    if (_numSectors == 0U)
    {
        _numSectors = 1U;
    }
    if (_numSectors > MAX_SECTORS)
    {
        _numSectors = MAX_SECTORS;
    }
    if ((_sectorSize == 0U) || (_sectorSize > MAX_SECTOR_SIZE))
    {
        _sectorSize = MAX_SECTOR_SIZE;
    }
    if (_maxEraseCycles == 0U)
    {
        _maxEraseCycles = 1U;
    }
}

bool Fee::init()
{
    size_t const total = static_cast<size_t>(_numSectors) * _sectorSize;
    if ((total == 0U) || (total > MAX_LOGICAL_SIZE))
    {
        return false;
    }
    _store.clear();
    _store.resize(total, 0xFFU);
    for (uint32_t i = 0U; i < MAX_SECTORS; ++i)
    {
        _eraseCycles[i] = 0U;
        _bad[i]         = false;
        _erased[i]      = true;
    }
    _writeCount  = 0U;
    _initialized = true;
    return true;
}

uint32_t Fee::sectorIndexFor(uint32_t logicalAddress) const
{
    if (_sectorSize == 0U)
    {
        return 0U;
    }
    return (logicalAddress / _sectorSize) % _numSectors;
}

uint32_t Fee::sectorBase(uint32_t sectorIndex) const { return sectorIndex * _sectorSize; }

uint32_t Fee::findBestSector(uint32_t hintSector) const
{
    uint32_t best      = hintSector % _numSectors;
    uint32_t bestCycles = UINT32_MAX;
    for (uint32_t i = 0U; i < _numSectors; ++i)
    {
        uint32_t const idx = (hintSector + i) % _numSectors;
        if (_bad[idx])
        {
            continue;
        }
        if (_eraseCycles[idx] < bestCycles)
        {
            bestCycles = _eraseCycles[idx];
            best       = idx;
        }
    }
    return best;
}

void Fee::touchSector(uint32_t sectorIndex)
{
    if (sectorIndex < MAX_SECTORS)
    {
        ++_eraseCycles[sectorIndex];
        _erased[sectorIndex] = false;
        ++_writeCount;
    }
}

bool Fee::read(uint32_t logicalAddress, uint8_t* data, size_t length) const
{
    if (!_initialized || (data == nullptr) || (length == 0U))
    {
        return false;
    }
    size_t const total = static_cast<size_t>(_numSectors) * _sectorSize;
    if ((static_cast<size_t>(logicalAddress) + length) > total)
    {
        return false;
    }
    // Data is mirrored by logical address: sector rotation only affects wear counters,
    // so a plain logical read is correct while writes rotate erase load.
    for (size_t i = 0U; i < length; ++i)
    {
        data[i] = _store[static_cast<size_t>(logicalAddress) + i];
    }
    return true;
}

bool Fee::write(uint32_t logicalAddress, uint8_t const* data, size_t length)
{
    if (!_initialized || (data == nullptr) || (length == 0U))
    {
        return false;
    }
    size_t const total = static_cast<size_t>(_numSectors) * _sectorSize;
    if ((static_cast<size_t>(logicalAddress) + length) > total)
    {
        return false;
    }
    uint32_t const hint = sectorIndexFor(logicalAddress);
    uint32_t const best = findBestSector(hint);
    if (_bad[best])
    {
        return false;
    }
    for (size_t i = 0U; i < length; ++i)
    {
        _store[static_cast<size_t>(logicalAddress) + i] = data[i];
    }
    touchSector(best);
    if (_eraseCycles[best] > _maxEraseCycles)
    {
        return false;
    }
    return true;
}

bool Fee::erase(uint32_t logicalAddress, size_t length)
{
    if (!_initialized || (length == 0U))
    {
        return false;
    }
    size_t const total = static_cast<size_t>(_numSectors) * _sectorSize;
    if ((static_cast<size_t>(logicalAddress) + length) > total)
    {
        return false;
    }
    for (size_t i = 0U; i < length; ++i)
    {
        _store[static_cast<size_t>(logicalAddress) + i] = 0xFFU;
    }
    touchSector(sectorIndexFor(logicalAddress));
    return true;
}

uint8_t Fee::getWearLevel() const
{
    uint32_t worst = 0U;
    for (uint32_t i = 0U; i < _numSectors; ++i)
    {
        if (_eraseCycles[i] > worst)
        {
            worst = _eraseCycles[i];
        }
    }
    uint32_t pct = (worst * 100U) / _maxEraseCycles;
    return static_cast<uint8_t>(pct > 100U ? 100U : pct);
}

uint32_t Fee::getEraseCycleCount(uint32_t sectorIndex) const
{
    if (sectorIndex >= MAX_SECTORS)
    {
        return 0U;
    }
    return _eraseCycles[sectorIndex];
}

bool Fee::markBadSector(uint32_t sectorIndex)
{
    if (sectorIndex >= _numSectors)
    {
        return false;
    }
    _bad[sectorIndex] = true;
    return true;
}

bool Fee::isSectorBad(uint32_t sectorIndex) const
{
    if (sectorIndex >= MAX_SECTORS)
    {
        return true;
    }
    return _bad[sectorIndex];
}

void Fee::runGarbageCollection()
{
    // Emulated flash: compact by re-balancing erase counters toward the mean.
    uint32_t sum = 0U;
    uint32_t good = 0U;
    for (uint32_t i = 0U; i < _numSectors; ++i)
    {
        if (!_bad[i])
        {
            sum += _eraseCycles[i];
            ++good;
        }
    }
    if (good == 0U)
    {
        return;
    }
    uint32_t const mean = sum / good;
    for (uint32_t i = 0U; i < _numSectors; ++i)
    {
        if (!_bad[i] && (_eraseCycles[i] > (mean + 1U)))
        {
            _eraseCycles[i] = mean + 1U;
        }
        _erased[i] = true;
    }
}

void Fee::mainFunction() { runGarbageCollection(); }

} // namespace nvram
