/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <integrity/Crc.h>
#include <integrity/IntegrityGuard.h>

namespace integrity
{

IntegrityGuard& IntegrityGuard::instance()
{
    static IntegrityGuard inst{};
    return inst;
}

bool IntegrityGuard::init(ProfileConfig const* profiles, size_t count)
{
    _states.clear();
    _initialized = false;
    if ((profiles == nullptr) || (count == 0U) || (count > MAX_PROFILES))
    {
        return false;
    }
    for (size_t i = 0U; i < count; ++i)
    {
        State st{};
        st.config   = profiles[i];
        st.txCounter = 0U;
        st.rxCounter = 0U;
        st.rxSynced  = false;
        _states[st.config.profileId] = st;
    }
    _initialized = true;
    return true;
}

IntegrityGuard::State* IntegrityGuard::find(uint8_t profileId)
{
    auto it = _states.find(profileId);
    if (it == _states.end())
    {
        return nullptr;
    }
    return &it->second;
}

IntegrityGuard::State const* IntegrityGuard::find(uint8_t profileId) const
{
    auto it = _states.find(profileId);
    if (it == _states.end())
    {
        return nullptr;
    }
    return &it->second;
}

uint32_t IntegrityGuard::now() const
{
    if (_tickProvider)
    {
        return _tickProvider();
    }
    return _tick;
}

uint32_t IntegrityGuard::maskFor(uint8_t bits)
{
    if (bits >= 32U)
    {
        return 0xFFFFFFFFUL;
    }
    return (bits == 0U) ? 0U : ((1UL << bits) - 1UL);
}

bool IntegrityGuard::validateCounter(uint32_t received, uint32_t expected, uint8_t bits, bool synced)
{
    uint32_t const mask = maskFor(bits);
    received &= mask;
    expected &= mask;
    if (!synced)
    {
        return true; // first frame after reset syncs unconditionally
    }
    if (received == expected)
    {
        return false; // duplicate
    }
    uint32_t const next = (expected + 1U) & mask;
    if (received == next)
    {
        return true;
    }
    // Accept forward gaps (lost frames), reject rollback / out-of-order.
    uint32_t const delta = (received - expected) & mask;
    return (delta <= (mask / 2U)) && (delta != 0U);
}

CheckResult IntegrityGuard::protectProfile1(uint8_t* data, uint16_t len, uint32_t dataId, bool inc)
{
    if ((data == nullptr) || (len < 2U))
    {
        return CheckResult::ConfigError;
    }
    static uint32_t ctr = 0U;
    (void)inc;
    // Stateless helper: caller manages counter via IntegrityGuard::protect for stateful use.
    // Encode: [data][ctrHi4 | dataIdNibble][CRC8 over data+header+dataId16BE].
    uint8_t header = static_cast<uint8_t>(((ctr & 0x0FU) << 4U) | ((dataId >> 12U) & 0x0FU));
    data[static_cast<size_t>(len) - 2U] = header;
    uint8_t buf[258U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 1U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    buf[pos++] = static_cast<uint8_t>(dataId & 0xFFU);
    data[static_cast<size_t>(len) - 1U] = crc8SaeJ1850(buf, pos);
    ctr = (ctr + 1U) & 0x0FU;
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::checkProfile1(uint8_t const* data, uint16_t len, uint32_t dataId)
{
    if ((data == nullptr) || (len < 2U))
    {
        return CheckResult::ConfigError;
    }
    uint8_t const header   = data[static_cast<size_t>(len) - 2U];
    uint8_t const rxNibble = static_cast<uint8_t>(header & 0x0FU);
    if (rxNibble != static_cast<uint8_t>((dataId >> 12U) & 0x0FU))
    {
        return CheckResult::DataIdError;
    }
    uint8_t buf[258U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 1U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    buf[pos++] = static_cast<uint8_t>(dataId & 0xFFU);
    if (crc8SaeJ1850(buf, pos) != data[static_cast<size_t>(len) - 1U])
    {
        return CheckResult::CrcError;
    }
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::protectProfile2(
    uint8_t* data, uint16_t len, uint8_t const* list, size_t listLen, bool inc)
{
    if ((data == nullptr) || (len < 2U) || (list == nullptr) || (listLen == 0U))
    {
        return CheckResult::ConfigError;
    }
    static uint32_t ctr = 0U;
    (void)inc;
    uint8_t const dataId = list[ctr % listLen];
    uint8_t header = static_cast<uint8_t>(((ctr & 0x0FU) << 4U) | (dataId & 0x0FU));
    data[static_cast<size_t>(len) - 2U] = header;
    uint8_t buf[257U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 1U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = dataId;
    data[static_cast<size_t>(len) - 1U] = crc8SaeJ1850(buf, pos);
    ctr = (ctr + 1U) & 0x0FU;
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::checkProfile2(
    uint8_t const* data, uint16_t len, uint8_t const* list, size_t listLen)
{
    if ((data == nullptr) || (len < 2U) || (list == nullptr) || (listLen == 0U))
    {
        return CheckResult::ConfigError;
    }
    uint8_t const header    = data[static_cast<size_t>(len) - 2U];
    uint8_t const rxCounter = static_cast<uint8_t>((header >> 4U) & 0x0FU);
    uint8_t const rxDataId  = list[rxCounter % listLen];
    uint8_t buf[257U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 1U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = rxDataId;
    if (crc8SaeJ1850(buf, pos) != data[static_cast<size_t>(len) - 1U])
    {
        return CheckResult::CrcError;
    }
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::protectProfile4(uint8_t* data, uint16_t len, uint32_t dataId, bool inc)
{
    // Layout: [payload(N-12)][len16BE][ctr16BE][dataId32BE][crc32BE], CRC over all but CRC.
    if ((data == nullptr) || (len < 12U))
    {
        return CheckResult::ConfigError;
    }
    (void)inc;
    static uint32_t ctr = 0U;
    size_t const hdr    = static_cast<size_t>(len) - 12U;
    data[hdr + 0U]      = static_cast<uint8_t>((len >> 8U) & 0xFFU);
    data[hdr + 1U]      = static_cast<uint8_t>(len & 0xFFU);
    data[hdr + 2U]      = static_cast<uint8_t>((ctr >> 8U) & 0xFFU);
    data[hdr + 3U]      = static_cast<uint8_t>(ctr & 0xFFU);
    data[hdr + 4U]      = static_cast<uint8_t>((dataId >> 24U) & 0xFFU);
    data[hdr + 5U]      = static_cast<uint8_t>((dataId >> 16U) & 0xFFU);
    data[hdr + 6U]      = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    data[hdr + 7U]      = static_cast<uint8_t>(dataId & 0xFFU);
    uint32_t const crc  = crc32Ieee(data, static_cast<size_t>(len) - 4U);
    data[hdr + 8U]      = static_cast<uint8_t>((crc >> 24U) & 0xFFU);
    data[hdr + 9U]      = static_cast<uint8_t>((crc >> 16U) & 0xFFU);
    data[hdr + 10U]     = static_cast<uint8_t>((crc >> 8U) & 0xFFU);
    data[hdr + 11U]     = static_cast<uint8_t>(crc & 0xFFU);
    ctr                 = (ctr + 1U) & 0xFFFFU;
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::checkProfile4(uint8_t const* data, uint16_t len, uint32_t dataId)
{
    if ((data == nullptr) || (len < 12U))
    {
        return CheckResult::ConfigError;
    }
    size_t const hdr = static_cast<size_t>(len) - 12U;
    uint32_t const rxDataId = (static_cast<uint32_t>(data[hdr + 4U]) << 24U)
        | (static_cast<uint32_t>(data[hdr + 5U]) << 16U)
        | (static_cast<uint32_t>(data[hdr + 6U]) << 8U) | data[hdr + 7U];
    if (rxDataId != dataId)
    {
        return CheckResult::DataIdError;
    }
    uint32_t const rxCrc = (static_cast<uint32_t>(data[hdr + 8U]) << 24U)
        | (static_cast<uint32_t>(data[hdr + 9U]) << 16U)
        | (static_cast<uint32_t>(data[hdr + 10U]) << 8U) | data[hdr + 11U];
    if (crc32Ieee(data, static_cast<size_t>(len) - 4U) != rxCrc)
    {
        return CheckResult::CrcError;
    }
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::protectProfile5(uint8_t* data, uint16_t len, uint32_t dataId, bool inc)
{
    // Layout: [payload(N-3)][ctr8][crc16BE], CRC over payload+ctr+dataId16BE.
    if ((data == nullptr) || (len < 3U))
    {
        return CheckResult::ConfigError;
    }
    (void)inc;
    static uint32_t ctr = 0U;
    data[static_cast<size_t>(len) - 3U] = static_cast<uint8_t>(ctr & 0xFFU);
    uint8_t buf[260U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 2U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    buf[pos++] = static_cast<uint8_t>(dataId & 0xFFU);
    uint16_t const crc                    = crc16CcittFalse(buf, pos);
    data[static_cast<size_t>(len) - 2U]   = static_cast<uint8_t>((crc >> 8U) & 0xFFU);
    data[static_cast<size_t>(len) - 1U]   = static_cast<uint8_t>(crc & 0xFFU);
    ctr                                   = (ctr + 1U) & 0xFFU;
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::checkProfile5(uint8_t const* data, uint16_t len, uint32_t dataId)
{
    if ((data == nullptr) || (len < 3U))
    {
        return CheckResult::ConfigError;
    }
    uint8_t buf[260U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 2U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    buf[pos++] = static_cast<uint8_t>(dataId & 0xFFU);
    uint16_t const calc = crc16CcittFalse(buf, pos);
    uint16_t const rx = static_cast<uint16_t>(
        (static_cast<uint16_t>(data[static_cast<size_t>(len) - 2U]) << 8U)
        | data[static_cast<size_t>(len) - 1U]);
    if (calc != rx)
    {
        return CheckResult::CrcError;
    }
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::protectProfile11(uint8_t* data, uint16_t len, uint32_t dataId, bool inc)
{
    // Layout: [payload(N-2)][ctr8][crc8], CRC over payload+ctr+dataId16BE.
    if ((data == nullptr) || (len < 2U))
    {
        return CheckResult::ConfigError;
    }
    (void)inc;
    static uint32_t ctr = 0U;
    data[static_cast<size_t>(len) - 2U] = static_cast<uint8_t>(ctr & 0xFFU);
    uint8_t buf[259U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 1U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    buf[pos++] = static_cast<uint8_t>(dataId & 0xFFU);
    data[static_cast<size_t>(len) - 1U] = crc8SaeJ1850(buf, pos);
    ctr                                 = (ctr + 1U) & 0xFFU;
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::checkProfile11(uint8_t const* data, uint16_t len, uint32_t dataId)
{
    if ((data == nullptr) || (len < 2U))
    {
        return CheckResult::ConfigError;
    }
    uint8_t buf[259U];
    size_t pos = 0U;
    for (uint16_t i = 0U; i < static_cast<uint16_t>(len - 1U); ++i)
    {
        buf[pos++] = data[i];
    }
    buf[pos++] = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    buf[pos++] = static_cast<uint8_t>(dataId & 0xFFU);
    if (crc8SaeJ1850(buf, pos) != data[static_cast<size_t>(len) - 1U])
    {
        return CheckResult::CrcError;
    }
    return CheckResult::Ok;
}

CheckResult IntegrityGuard::protect(uint8_t profileId, uint8_t* data, uint16_t length, bool increment)
{
    State* st = find(profileId);
    if ((st == nullptr) || (data == nullptr))
    {
        return CheckResult::ConfigError;
    }
    CheckResult r = CheckResult::ConfigError;
    switch (st->config.type)
    {
        case ProfileType::P01:
            // Stateful counter: temporarily sync the stateless helper via repeated calls is not
            // possible, so implement inline with txCounter.
            {
                if (length < 2U)
                {
                    return CheckResult::ConfigError;
                }
                if (increment)
                {
                    st->txCounter = (st->txCounter + 1U) & 0x0FU;
                }
                uint32_t const ctr = st->txCounter;
                data[length - 2U]  = static_cast<uint8_t>(
                    ((ctr & 0x0FU) << 4U) | ((st->config.dataId >> 12U) & 0x0FU));
                uint8_t buf[258U];
                size_t pos = 0U;
                for (uint16_t i = 0U; i < static_cast<uint16_t>(length - 1U); ++i)
                {
                    buf[pos++] = data[i];
                }
                buf[pos++]         = static_cast<uint8_t>((st->config.dataId >> 8U) & 0xFFU);
                buf[pos++]         = static_cast<uint8_t>(st->config.dataId & 0xFFU);
                data[length - 1U]  = crc8SaeJ1850(buf, pos);
                r                  = CheckResult::Ok;
            }
            break;
        case ProfileType::P02:
        {
            if ((length < 2U) || st->config.dataIdList.empty())
            {
                return CheckResult::ConfigError;
            }
            if (increment)
            {
                st->txCounter = (st->txCounter + 1U) & 0x0FU;
            }
            uint32_t const ctr  = st->txCounter;
            uint8_t const dataId = st->config.dataIdList[ctr % st->config.dataIdList.size()];
            data[length - 2U]     = static_cast<uint8_t>(((ctr & 0x0FU) << 4U) | (dataId & 0x0FU));
            uint8_t buf[257U];
            size_t pos = 0U;
            for (uint16_t i = 0U; i < static_cast<uint16_t>(length - 1U); ++i)
            {
                buf[pos++] = data[i];
            }
            buf[pos++]        = dataId;
            data[length - 1U] = crc8SaeJ1850(buf, pos);
            r                 = CheckResult::Ok;
            break;
        }
        case ProfileType::P04:
            if (increment)
            {
                st->txCounter = (st->txCounter + 1U) & 0xFFFFU;
            }
            {
                uint32_t const saved = st->txCounter;
                // Reuse layout with explicit counter value.
                if (length < 12U)
                {
                    return CheckResult::ConfigError;
                }
                size_t const hdr = static_cast<size_t>(length) - 12U;
                data[hdr + 0U]    = static_cast<uint8_t>((length >> 8U) & 0xFFU);
                data[hdr + 1U]    = static_cast<uint8_t>(length & 0xFFU);
                data[hdr + 2U]    = static_cast<uint8_t>((saved >> 8U) & 0xFFU);
                data[hdr + 3U]    = static_cast<uint8_t>(saved & 0xFFU);
                data[hdr + 4U]    = static_cast<uint8_t>((st->config.dataId >> 24U) & 0xFFU);
                data[hdr + 5U]    = static_cast<uint8_t>((st->config.dataId >> 16U) & 0xFFU);
                data[hdr + 6U]    = static_cast<uint8_t>((st->config.dataId >> 8U) & 0xFFU);
                data[hdr + 7U]    = static_cast<uint8_t>(st->config.dataId & 0xFFU);
                uint32_t const crc = crc32Ieee(data, static_cast<size_t>(length) - 4U);
                data[hdr + 8U]    = static_cast<uint8_t>((crc >> 24U) & 0xFFU);
                data[hdr + 9U]    = static_cast<uint8_t>((crc >> 16U) & 0xFFU);
                data[hdr + 10U]   = static_cast<uint8_t>((crc >> 8U) & 0xFFU);
                data[hdr + 11U]   = static_cast<uint8_t>(crc & 0xFFU);
                r                 = CheckResult::Ok;
            }
            break;
        case ProfileType::P05:
            if (increment)
            {
                st->txCounter = (st->txCounter + 1U) & 0xFFU;
            }
            {
                if (length < 3U)
                {
                    return CheckResult::ConfigError;
                }
                data[length - 3U] = static_cast<uint8_t>(st->txCounter & 0xFFU);
                uint8_t buf[260U];
                size_t pos = 0U;
                for (uint16_t i = 0U; i < static_cast<uint16_t>(length - 2U); ++i)
                {
                    buf[pos++] = data[i];
                }
                buf[pos++]       = static_cast<uint8_t>((st->config.dataId >> 8U) & 0xFFU);
                buf[pos++]       = static_cast<uint8_t>(st->config.dataId & 0xFFU);
                uint16_t const c = crc16CcittFalse(buf, pos);
                data[length - 2U] = static_cast<uint8_t>((c >> 8U) & 0xFFU);
                data[length - 1U] = static_cast<uint8_t>(c & 0xFFU);
                r                 = CheckResult::Ok;
            }
            break;
        case ProfileType::P11:
            if (increment)
            {
                st->txCounter = (st->txCounter + 1U) & 0xFFU;
            }
            {
                if (length < 2U)
                {
                    return CheckResult::ConfigError;
                }
                data[length - 2U] = static_cast<uint8_t>(st->txCounter & 0xFFU);
                uint8_t buf[259U];
                size_t pos = 0U;
                for (uint16_t i = 0U; i < static_cast<uint16_t>(length - 1U); ++i)
                {
                    buf[pos++] = data[i];
                }
                buf[pos++]      = static_cast<uint8_t>((st->config.dataId >> 8U) & 0xFFU);
                buf[pos++]      = static_cast<uint8_t>(st->config.dataId & 0xFFU);
                data[length - 1U] = crc8SaeJ1850(buf, pos);
                r                 = CheckResult::Ok;
            }
            break;
    }
    return r;
}

CheckResult IntegrityGuard::check(uint8_t profileId, uint8_t const* data, uint16_t length)
{
    State* st = find(profileId);
    if ((st == nullptr) || (data == nullptr))
    {
        return CheckResult::ConfigError;
    }
    if ((st->config.timeoutMs != 0U) && st->rxSynced)
    {
        uint32_t const elapsed = now() - st->lastRxTick;
        if (elapsed > st->config.timeoutMs)
        {
            st->rxSynced = false;
            return CheckResult::TimeoutError;
        }
    }
    CheckResult crcRes = CheckResult::ConfigError;
    uint32_t rxCounter = 0U;
    switch (st->config.type)
    {
        case ProfileType::P01:
            crcRes = checkProfile1(data, length, st->config.dataId);
            if (length >= 2U)
            {
                rxCounter = (data[length - 2U] >> 4U) & 0x0FU;
            }
            break;
        case ProfileType::P02:
            if (st->config.dataIdList.empty())
            {
                return CheckResult::ConfigError;
            }
            crcRes = checkProfile2(
                data, length, st->config.dataIdList.data(),
                st->config.dataIdList.size());
            if (length >= 2U)
            {
                rxCounter = (data[length - 2U] >> 4U) & 0x0FU;
            }
            break;
        case ProfileType::P04:
            crcRes = checkProfile4(data, length, st->config.dataId);
            if (length >= 12U)
            {
                size_t const hdr = static_cast<size_t>(length) - 12U;
                rxCounter = (static_cast<uint32_t>(data[hdr + 2U]) << 8U) | data[hdr + 3U];
            }
            break;
        case ProfileType::P05:
            crcRes = checkProfile5(data, length, st->config.dataId);
            if (length >= 3U)
            {
                rxCounter = data[length - 3U];
            }
            break;
        case ProfileType::P11:
            crcRes = checkProfile11(data, length, st->config.dataId);
            if (length >= 2U)
            {
                rxCounter = data[length - 2U];
            }
            break;
    }
    if (crcRes != CheckResult::Ok)
    {
        return crcRes;
    }
    uint8_t const bits
        = (st->config.type == ProfileType::P01) || (st->config.type == ProfileType::P02)
        ? 4U
        : (st->config.type == ProfileType::P04 ? 16U : 8U);
    uint32_t const mask = maskFor(bits);
    if (!validateCounter(rxCounter, st->rxCounter, bits, st->rxSynced))
    {
        return CheckResult::CounterError;
    }
    st->rxCounter   = rxCounter & mask;
    st->rxSynced    = true;
    st->lastRxTick  = now();
    return CheckResult::Ok;
}

uint32_t IntegrityGuard::getCounter(uint8_t profileId) const
{
    State const* st = find(profileId);
    if (st == nullptr)
    {
        return 0U;
    }
    return st->txCounter;
}

void IntegrityGuard::reset(uint8_t profileId)
{
    State* st = find(profileId);
    if (st != nullptr)
    {
        st->txCounter  = 0U;
        st->rxCounter  = 0U;
        st->rxSynced   = false;
        st->lastRxTick = now();
    }
}

void IntegrityGuard::mainFunction()
{
    ++_tick;
    for (auto& kv : _states)
    {
        (void)kv;
    }
}

} // namespace integrity
