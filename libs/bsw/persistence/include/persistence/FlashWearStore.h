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
 * \ingroup persistence
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <etl/array.h>
#include <etl/vector.h>

namespace persistence
{

/// Flash EEPROM emulation with sector-rotation wear leveling.
///
/// Logical addresses are striped across N sectors; each write picks the valid
/// sector with the lowest erase-cycle count, so erase wear spreads evenly.
/// A bad sector can be marked and is then skipped for new writes.
class FlashWearStore
{
public:
    static constexpr size_t MAX_SECTORS      = 8U;
    static constexpr size_t MAX_SECTOR_SIZE  = 4096U;
    static constexpr size_t MAX_LOGICAL_SIZE = MAX_SECTORS * MAX_SECTOR_SIZE;

    FlashWearStore() = default;
    FlashWearStore(uint32_t numSectors, uint32_t sectorSize, uint32_t maxEraseCycles = 100000U);

    bool init();
    bool isInitialized() const { return _initialized; }

    bool read(uint32_t logicalAddress, uint8_t* data, size_t length) const;
    bool write(uint32_t logicalAddress, uint8_t const* data, size_t length);
    bool erase(uint32_t logicalAddress, size_t length);

    /// Worst-case wear as 0..100%.
    uint8_t getWearLevel() const;
    uint32_t getEraseCycleCount(uint32_t sectorIndex) const;
    uint32_t getWriteCount() const { return _writeCount; }

    bool markBadSector(uint32_t sectorIndex);
    bool isSectorBad(uint32_t sectorIndex) const;

    void runGarbageCollection();
    void mainFunction();

private:
    uint32_t sectorIndexFor(uint32_t logicalAddress) const;
    uint32_t sectorBase(uint32_t sectorIndex) const;
    uint32_t findBestSector(uint32_t hintSector) const;
    void touchSector(uint32_t sectorIndex);

    uint32_t _numSectors{4U};
    uint32_t _sectorSize{1024U};
    uint32_t _maxEraseCycles{100000U};
    bool _initialized{false};
    uint32_t _writeCount{0U};
    etl::array<uint32_t, MAX_SECTORS> _eraseCycles{};
    etl::array<bool, MAX_SECTORS> _bad{};
    etl::array<bool, MAX_SECTORS> _erased{};
    etl::vector<uint8_t, MAX_LOGICAL_SIZE> _store{};
};

} // namespace persistence
