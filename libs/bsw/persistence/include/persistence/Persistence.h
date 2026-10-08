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
#include <etl/delegate.h>
#include <etl/map.h>
#include <etl/string.h>
#include <etl/vector.h>

namespace persistence
{

static constexpr size_t MAX_BLOCKS        = 16U;
static constexpr size_t MAX_BLOCK_SIZE    = 8192U;
static constexpr size_t MAX_BLOCK_NAME_LEN = 31U;

/// Persistence operation result.
enum class Result : uint8_t
{
    Ok,
    Pending,
    BlockInvalid,
    DeviceError,
    CrcError,
    BlockFull,
    WriteProtected,
    DeviceBusy,
    WearLimitReached
};

enum class StorageType : uint8_t
{
    RamOnly,
    FlashOnly,
    RamAndFlash
};

enum class Redundancy : uint8_t
{
    Single,
    Redundant
};

enum class CrcKind : uint8_t
{
    None,
    Crc8,
    Crc16,
    Crc32
};

enum class Recovery : uint8_t
{
    UseDefault,
    UseRecovery,
    KeepOld
};

/// Static block descriptor (normally generated from YAML -> blob).
struct BlockConfig
{
    uint16_t blockId{0U};
    etl::string<MAX_BLOCK_NAME_LEN> name{};
    uint16_t size{0U};
    uint8_t deviceId{0U};
    StorageType storageType{StorageType::RamAndFlash};
    bool crcEnabled{true};
    CrcKind crcKind{CrcKind::Crc32};
    bool immediateWrite{false};
    Redundancy redundancy{Redundancy::Single};
    uint8_t defaultData{0x00U};
    Recovery recovery{Recovery::UseDefault};
    uint32_t writeCountLimit{0U}; // 0 = unlimited
};

using AsyncCallback = etl::delegate<void(Result)>;

/// Block-based Persistence manager with RAM mirror, CRC protection and deferred writes.
class Persistence
{
public:
    static Persistence& instance();

    Persistence(Persistence const&)            = delete;
    Persistence& operator=(Persistence const&) = delete;

    bool init(BlockConfig const* blocks, size_t count);
    bool isInitialized() const { return _initialized; }

    Result read(uint16_t blockId, void* data, uint16_t length, uint16_t offset = 0U) const;
    Result write(uint16_t blockId, void const* data, uint16_t length, uint16_t offset = 0U);

    Result readAsync(
        uint16_t blockId, void* data, uint16_t length, AsyncCallback callback,
        uint16_t offset = 0U) const;
    Result writeAsync(
        uint16_t blockId, void const* data, uint16_t length, AsyncCallback callback,
        uint16_t offset = 0U);

    Result invalidateBlock(uint16_t blockId);
    Result setToDefault(uint16_t blockId);
    bool getBlockInfo(uint16_t blockId, BlockConfig& info) const;
    uint32_t getWriteCounter(uint16_t blockId) const;
    uint8_t getWearLevel() const;

    /// Flush all deferred (non-immediate) blocks to flash. Call before Phase-5 SLEEP.
    Result shutdown();
    void mainFunction();

    /// Phase-5 hook: while true, deferred writes return DeviceBusy (immediate writes unaffected).
    void setDeferredWritesBlocked(bool blocked) { _deferredBlocked = blocked; }

    /// Test hook: corrupt one flash byte so the next read trips CRC recovery.
    bool corruptFlashForTest(uint16_t blockId, uint16_t offset, uint8_t xorMask);

private:
    Persistence() = default;

    struct BlockState
    {
        BlockConfig config{};
        etl::vector<uint8_t, MAX_BLOCK_SIZE> ram{};
        etl::vector<uint8_t, MAX_BLOCK_SIZE> flash0{};
        etl::vector<uint8_t, MAX_BLOCK_SIZE> flash1{};
        etl::vector<uint8_t, 4U> crc0{};
        etl::vector<uint8_t, 4U> crc1{};
        uint32_t writeCounter{0U};
        uint32_t eraseCycles{0U};
        bool dirty{false};
        bool valid{false};
    };

    BlockState* find(uint16_t blockId);
    BlockState const* find(uint16_t blockId) const;
    static size_t crcSize(CrcKind kind);
    static void computeCrc(BlockState const& st, uint8_t const* data, uint8_t* out);
    static bool checkCrc(BlockState const& st, uint8_t const* data, uint8_t const* stored);
    Result writeToFlash(BlockState& st, uint8_t const* fullImage);
    Result readFromFlash(BlockState const& st, uint8_t* out) const;

    etl::map<uint16_t, BlockState, MAX_BLOCKS> _blocks{};
    bool _initialized{false};
    bool _deferredBlocked{false};
    uint32_t _maxEraseCycles{100000U};
};

} // namespace persistence
