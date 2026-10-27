/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <persistence/Crc.h>
#include <persistence/Persistence.h>

namespace persistence
{

Persistence& Persistence::instance()
{
    static Persistence inst{};
    return inst;
}

bool Persistence::init(BlockConfig const* blocks, size_t count)
{
    _blocks.clear();
    _initialized      = false;
    _deferredBlocked  = false;
    if ((blocks == nullptr) || (count == 0U) || (count > MAX_BLOCKS))
    {
        return false;
    }
    for (size_t i = 0U; i < count; ++i)
    {
        BlockConfig const& cfg = blocks[i];
        if ((cfg.size == 0U) || (cfg.size > MAX_BLOCK_SIZE))
        {
            return false;
        }
        BlockState st{};
        st.config = cfg;
        st.ram.clear();
        st.ram.resize(cfg.size, cfg.defaultData);
        st.flash0.clear();
        st.flash0.resize(cfg.size, cfg.defaultData);
        st.flash1.clear();
        st.flash1.resize(cfg.size, cfg.defaultData);
        st.crc0.clear();
        st.crc0.resize(crcSize(cfg.crcKind), 0U);
        st.crc1.clear();
        st.crc1.resize(crcSize(cfg.crcKind), 0U);
        st.writeCounter = 0U;
        st.eraseCycles  = 0U;
        st.dirty        = false;
        st.valid        = true;
        computeCrc(st, st.flash0.data(), st.crc0.data());
        if (cfg.redundancy == Redundancy::Redundant)
        {
            computeCrc(st, st.flash1.data(), st.crc1.data());
        }
        _blocks[cfg.blockId] = st;
    }
    _initialized = true;
    return true;
}

Persistence::BlockState* Persistence::find(uint16_t blockId)
{
    auto it = _blocks.find(blockId);
    if (it == _blocks.end())
    {
        return nullptr;
    }
    return &it->second;
}

Persistence::BlockState const* Persistence::find(uint16_t blockId) const
{
    auto it = _blocks.find(blockId);
    if (it == _blocks.end())
    {
        return nullptr;
    }
    return &it->second;
}

size_t Persistence::crcSize(CrcKind kind)
{
    switch (kind)
    {
        case CrcKind::Crc8: return 1U;
        case CrcKind::Crc16: return 2U;
        case CrcKind::Crc32: return 4U;
        case CrcKind::None:
        default: return 0U;
    }
}

void Persistence::computeCrc(BlockState const& st, uint8_t const* data, uint8_t* out)
{
    if (!st.config.crcEnabled)
    {
        return;
    }
    switch (st.config.crcKind)
    {
        case CrcKind::Crc8:
            out[0U] = crc8SaeJ1850(data, st.config.size);
            break;
        case CrcKind::Crc16:
        {
            uint16_t const v = crc16CcittFalse(data, st.config.size);
            out[0U]          = static_cast<uint8_t>((v >> 8U) & 0xFFU);
            out[1U]          = static_cast<uint8_t>(v & 0xFFU);
            break;
        }
        case CrcKind::Crc32:
        {
            uint32_t const v = crc32Ieee(data, st.config.size);
            out[0U]          = static_cast<uint8_t>((v >> 24U) & 0xFFU);
            out[1U]          = static_cast<uint8_t>((v >> 16U) & 0xFFU);
            out[2U]          = static_cast<uint8_t>((v >> 8U) & 0xFFU);
            out[3U]          = static_cast<uint8_t>(v & 0xFFU);
            break;
        }
        case CrcKind::None:
        default: break;
    }
}

bool Persistence::checkCrc(BlockState const& st, uint8_t const* data, uint8_t const* stored)
{
    if (!st.config.crcEnabled)
    {
        return true;
    }
    uint8_t calc[4U] = {0U, 0U, 0U, 0U};
    computeCrc(st, data, calc);
    size_t const n = crcSize(st.config.crcKind);
    for (size_t i = 0U; i < n; ++i)
    {
        if (calc[i] != stored[i])
        {
            return false;
        }
    }
    return true;
}

Result Persistence::writeToFlash(BlockState& st, uint8_t const* fullImage)
{
    if (st.eraseCycles >= _maxEraseCycles)
    {
        return Result::WearLimitReached;
    }
    for (uint16_t i = 0U; i < st.config.size; ++i)
    {
        st.flash0[i] = fullImage[i];
    }
    computeCrc(st, st.flash0.data(), st.crc0.data());
    if (st.config.redundancy == Redundancy::Redundant)
    {
        for (uint16_t i = 0U; i < st.config.size; ++i)
        {
            st.flash1[i] = fullImage[i];
        }
        computeCrc(st, st.flash1.data(), st.crc1.data());
    }
    ++st.eraseCycles;
    ++st.writeCounter;
    if ((st.config.writeCountLimit != 0U) && (st.writeCounter > st.config.writeCountLimit))
    {
        return Result::WearLimitReached;
    }
    st.dirty = false;
    return Result::Ok;
}

Result Persistence::readFromFlash(BlockState const& st, uint8_t* out) const
{
    bool const ok0 = checkCrc(st, st.flash0.data(), st.crc0.data());
    if (ok0)
    {
        for (uint16_t i = 0U; i < st.config.size; ++i)
        {
            out[i] = st.flash0[i];
        }
        return Result::Ok;
    }
    if (st.config.redundancy == Redundancy::Redundant)
    {
        bool const ok1 = checkCrc(st, st.flash1.data(), st.crc1.data());
        if (ok1)
        {
            for (uint16_t i = 0U; i < st.config.size; ++i)
            {
                out[i] = st.flash1[i];
            }
            return Result::Ok;
        }
    }
    return Result::CrcError;
}

Result Persistence::read(uint16_t blockId, void* data, uint16_t length, uint16_t offset) const
{
    if (!_initialized || (data == nullptr))
    {
        return Result::DeviceError;
    }
    BlockState const* st = find(blockId);
    if (st == nullptr)
    {
        return Result::BlockInvalid;
    }
    if (((static_cast<uint32_t>(offset) + length) > st->config.size) || (length == 0U))
    {
        return Result::BlockFull;
    }
    // Validate backing flash CRC first so bit flips are detected even though reads
    // are served from the RAM mirror.
    uint8_t const* flashImg  = st->flash0.data();
    uint8_t const* flashCrc  = st->crc0.data();
    bool flashOk             = checkCrc(*st, flashImg, flashCrc);
    bool redundantOk         = false;
    uint8_t const* goodFlash = nullptr;
    if (!flashOk && (st->config.redundancy == Redundancy::Redundant))
    {
        redundantOk = checkCrc(*st, st->flash1.data(), st->crc1.data());
        if (redundantOk)
        {
            goodFlash = st->flash1.data();
        }
    }
    else if (flashOk)
    {
        goodFlash = flashImg;
    }
    if (goodFlash == nullptr)
    {
        // CRC failure: apply recovery policy for the served bytes.
        if (st->config.recovery == Recovery::UseDefault)
        {
            auto* out = static_cast<uint8_t*>(data);
            for (uint16_t i = 0U; i < length; ++i)
            {
                out[i] = st->config.defaultData;
            }
        }
        else if ((st->config.recovery == Recovery::UseRecovery)
                 && (st->config.redundancy == Redundancy::Redundant))
        {
            auto* out = static_cast<uint8_t*>(data);
            for (uint16_t i = 0U; i < length; ++i)
            {
                out[i] = st->flash1[static_cast<size_t>(offset) + i];
            }
        }
        return Result::CrcError;
    }
    // RAM mirror is authoritative when not dirty w.r.t. flash; serve from RAM.
    auto const* src = static_cast<uint8_t const*>(st->ram.data());
    auto* out       = static_cast<uint8_t*>(data);
    for (uint16_t i = 0U; i < length; ++i)
    {
        out[i] = src[static_cast<size_t>(offset) + i];
    }
    (void)redundantOk;
    return Result::Ok;
}

Result Persistence::write(uint16_t blockId, void const* data, uint16_t length, uint16_t offset)
{
    if (!_initialized || (data == nullptr))
    {
        return Result::DeviceError;
    }
    BlockState* st = find(blockId);
    if (st == nullptr)
    {
        return Result::BlockInvalid;
    }
    if (((static_cast<uint32_t>(offset) + length) > st->config.size) || (length == 0U))
    {
        return Result::BlockFull;
    }
    auto const* src = static_cast<uint8_t const*>(data);
    for (uint16_t i = 0U; i < length; ++i)
    {
        st->ram[static_cast<size_t>(offset) + i] = src[i];
    }
    if (st->config.immediateWrite)
    {
        return writeToFlash(*st, st->ram.data());
    }
    if (_deferredBlocked)
    {
        st->dirty = true;
        return Result::DeviceBusy;
    }
    st->dirty = true;
    return Result::Ok;
}

Result Persistence::readAsync(
    uint16_t blockId, void* data, uint16_t length, AsyncCallback callback, uint16_t offset) const
{
    Result const r = read(blockId, data, length, offset);
    if (callback)
    {
        callback(r);
    }
    if ((r == Result::Ok))
    {
        return Result::Ok;
    }
    return r;
}

Result Persistence::writeAsync(
    uint16_t blockId, void const* data, uint16_t length, AsyncCallback callback, uint16_t offset)
{
    Result const r = write(blockId, data, length, offset);
    if (callback)
    {
        callback(r);
    }
    return r;
}

Result Persistence::invalidateBlock(uint16_t blockId)
{
    BlockState* st = find(blockId);
    if (st == nullptr)
    {
        return Result::BlockInvalid;
    }
    etl::vector<uint8_t, MAX_BLOCK_SIZE> img{};
    img.resize(st->config.size, 0U);
    Result const r = readFromFlash(*st, img.data());
    if (r != Result::Ok)
    {
        return r;
    }
    for (uint16_t i = 0U; i < st->config.size; ++i)
    {
        st->ram[i] = img[i];
    }
    st->dirty = false;
    return Result::Ok;
}

Result Persistence::setToDefault(uint16_t blockId)
{
    BlockState* st = find(blockId);
    if (st == nullptr)
    {
        return Result::BlockInvalid;
    }
    for (uint16_t i = 0U; i < st->config.size; ++i)
    {
        st->ram[i] = st->config.defaultData;
    }
    if (st->config.storageType == StorageType::RamOnly)
    {
        st->dirty = false;
        return Result::Ok;
    }
    return writeToFlash(*st, st->ram.data());
}

bool Persistence::getBlockInfo(uint16_t blockId, BlockConfig& info) const
{
    BlockState const* st = find(blockId);
    if (st == nullptr)
    {
        return false;
    }
    info = st->config;
    return true;
}

uint32_t Persistence::getWriteCounter(uint16_t blockId) const
{
    BlockState const* st = find(blockId);
    if (st == nullptr)
    {
        return 0U;
    }
    return st->writeCounter;
}

uint8_t Persistence::getWearLevel() const
{
    uint32_t worst = 0U;
    for (auto const& kv : _blocks)
    {
        if (kv.second.eraseCycles > worst)
        {
            worst = kv.second.eraseCycles;
        }
    }
    uint32_t const pct = (worst * 100U) / _maxEraseCycles;
    return static_cast<uint8_t>(pct > 100U ? 100U : pct);
}

Result Persistence::shutdown()
{
    if (!_initialized)
    {
        return Result::DeviceError;
    }
    for (auto& kv : _blocks)
    {
        BlockState& st = kv.second;
        if (st.dirty && (st.config.storageType != StorageType::RamOnly))
        {
            Result const r = writeToFlash(st, st.ram.data());
            if ((r != Result::Ok))
            {
                return r;
            }
        }
    }
    return Result::Ok;
}

void Persistence::mainFunction()
{
    // Background: nothing to do for the emulated flash; hook reserved for real HAL
    // wear-leveling ticks (FlashWearStore::mainFunction per device).
}

bool Persistence::corruptFlashForTest(uint16_t blockId, uint16_t offset, uint8_t xorMask)
{
    BlockState* st = find(blockId);
    if ((st == nullptr) || (offset >= st->config.size))
    {
        return false;
    }
    st->flash0[offset] = static_cast<uint8_t>(st->flash0[offset] ^ xorMask);
    return true;
}

} // namespace persistence
