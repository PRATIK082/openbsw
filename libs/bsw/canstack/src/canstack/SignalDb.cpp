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
 * \ingroup canstack
 */
#include "canstack/SignalDb.h"
#include "canstack/CanStackLogger.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace canstack
{
namespace logger = ::util::logger;

namespace
{

/// Extracts up to 64 raw bits following the DBC bit numbering rules.
uint64_t extractRaw(uint8_t const* frameData, uint8_t startBit, uint8_t length, bool isMotorola)
{
    uint64_t raw = 0U;

    if (!isMotorola)
    {
        // Intel: linear bit positions, LSB first.
        for (uint8_t i = 0U; i < length; ++i)
        {
            uint16_t const pos = static_cast<uint16_t>(startBit) + i;
            uint8_t const bit = (frameData[pos / 8U] >> (pos % 8U)) & 1U;
            raw |= static_cast<uint64_t>(bit) << i;
        }
    }
    else
    {
        // Motorola: DBC sawtooth walk starting at the MSB.
        uint16_t pos = startBit;
        for (uint8_t i = 0U; i < length; ++i)
        {
            uint8_t const bit = (frameData[pos / 8U] >> (pos % 8U)) & 1U;
            raw = (raw << 1U) | bit;
            pos = ((pos % 8U) == 0U) ? static_cast<uint16_t>(pos + 15U)
                                    : static_cast<uint16_t>(pos - 1U);
        }
    }

    return raw;
}

/// Inserts up to 64 raw bits following the DBC bit numbering rules.
void insertRaw(
    uint8_t* frameData, uint8_t startBit, uint8_t length, bool isMotorola, uint64_t raw)
{
    uint64_t const mask = (length >= 64U) ? ~0ULL : ((1ULL << length) - 1ULL);
    raw &= mask;

    if (!isMotorola)
    {
        for (uint8_t i = 0U; i < length; ++i)
        {
            uint16_t const pos = static_cast<uint16_t>(startBit) + i;
            uint8_t const bit = static_cast<uint8_t>((raw >> i) & 1U);
            uint8_t& byte = frameData[pos / 8U];
            byte = static_cast<uint8_t>(
                (byte & ~(1U << (pos % 8U))) | (bit << (pos % 8U)));
        }
    }
    else
    {
        uint16_t pos = startBit;
        for (uint8_t i = 0U; i < length; ++i)
        {
            // Bits are consumed MSB first.
            uint8_t const bit = static_cast<uint8_t>((raw >> (length - 1U - i)) & 1U);
            uint8_t& byte = frameData[pos / 8U];
            byte = static_cast<uint8_t>(
                (byte & ~(1U << (pos % 8U))) | (bit << (pos % 8U)));
            pos = ((pos % 8U) == 0U) ? static_cast<uint16_t>(pos + 15U)
                                    : static_cast<uint16_t>(pos - 1U);
        }
    }
}

double rawToPhysical(uint64_t raw, SignalConfig const& signal)
{
    int64_t signedRaw = static_cast<int64_t>(raw);

    if (signal.isSigned)
    {
        uint64_t const signBit = (signal.length >= 64U) ? (1ULL << 63U)
                                                        : (1ULL << (signal.length - 1U));
        if ((raw & signBit) != 0U)
        {
            uint64_t const range
                = (signal.length >= 64U) ? ~0ULL : ((1ULL << signal.length) - 1ULL);
            signedRaw = static_cast<int64_t>(raw) - static_cast<int64_t>(range) - 1;
        }
    }

    return (static_cast<double>(signedRaw) * signal.factor) + signal.offset;
}

} // namespace

bool SignalDatabase::loadFromDbc(std::string const& dbcFilePath, uint8_t const channelId)
{
    std::ifstream file(dbcFilePath.c_str());
    if (!file.is_open())
    {
        logger::Logger::error(logger::CANSTACK, "Cannot open DBC file %s", dbcFilePath.c_str());
        return false;
    }

    clear();

    std::string line;
    while (std::getline(file, line))
    {
        (void)parseDbcLine(line, channelId);
    }

    logger::Logger::info(
        logger::CANSTACK,
        "DBC %s loaded: %u frames",
        dbcFilePath.c_str(),
        static_cast<unsigned>(m_frames.size()));

    return !m_frames.empty();
}

void SignalDatabase::loadFrameTable(FrameTableEntry const* entries, size_t const count)
{
    for (size_t i = 0U; i < count; ++i)
    {
        FrameTableEntry const& entry = entries[i];
        auto const key = std::make_pair(entry.channelId, entry.frameId);

        auto it = m_frames.find(key);
        if (it == m_frames.end())
        {
            FrameConfig frame;
            frame.frameId    = entry.frameId;
            frame.frameName  = (entry.frameName != nullptr) ? entry.frameName : "";
            frame.dlc        = entry.dlc;
            frame.channelId  = entry.channelId;
            m_frames[key]    = frame;
        }
        else
        {
            // A frame created implicitly by a signal table gets its metadata now.
            it->second.frameName = (entry.frameName != nullptr) ? entry.frameName
                                                                : it->second.frameName;
            if (entry.dlc > 0U)
            {
                it->second.dlc = entry.dlc;
            }
        }
    }
}

void SignalDatabase::loadSignalTable(SignalTableEntry const* entries, size_t const count)
{
    for (size_t i = 0U; i < count; ++i)
    {
        SignalTableEntry const& entry = entries[i];

        SignalConfig signal;
        signal.signalName = (entry.signalName != nullptr) ? entry.signalName : "";
        signal.frameId    = entry.frameId;
        signal.startBit   = entry.startBit;
        signal.length     = entry.length;
        signal.isMotorola = entry.isMotorola;
        signal.factor     = (entry.factor != 0.0) ? entry.factor : 1.0;
        signal.offset     = entry.offset;
        signal.min        = entry.min;
        signal.max        = entry.max;
        signal.unit       = (entry.unit != nullptr) ? entry.unit : "";
        signal.channelId  = entry.channelId;
        signal.isMultiplexerSwitch = entry.isMultiplexerSwitch;
        signal.multiplexValue      = entry.multiplexValue;
        signal.isSigned            = entry.isSigned;

        storeSignal(signal);
    }
}

void SignalDatabase::clear()
{
    m_frames.clear();
    m_signalsByName.clear();
}

FrameConfig const* SignalDatabase::getFrameByChannelAndId(uint8_t const channelId, uint32_t const frameId) const
{
    auto const it = m_frames.find(std::make_pair(channelId, frameId));
    return (it != m_frames.end()) ? &it->second : nullptr;
}

SignalConfig const* SignalDatabase::getSignalByName(std::string const& signalName) const
{
    auto const it = m_signalsByName.find(signalName);
    return (it != m_signalsByName.end()) ? &it->second : nullptr;
}

void SignalDatabase::packSignal(uint8_t* frameData, SignalConfig const& signal, double value) const
{
    if ((frameData == nullptr) || (signal.length == 0U) || (signal.length > SIGNAL_MAX_BIT_LENGTH))
    {
        return;
    }

    double clamped = value;
    if (clamped < signal.min)
    {
        clamped = signal.min;
    }
    else if (clamped > signal.max)
    {
        clamped = signal.max;
    }

    double const scaled = (clamped - signal.offset) / signal.factor;
    int64_t raw = static_cast<int64_t>(std::llround(scaled));

    uint64_t const mask
        = (signal.length >= 64U) ? ~0ULL : ((1ULL << signal.length) - 1ULL);
    uint64_t rawBits = static_cast<uint64_t>(raw) & mask;

    insertRaw(frameData, signal.startBit, signal.length, signal.isMotorola, rawBits);
}

double SignalDatabase::unpackSignal(uint8_t const* frameData, SignalConfig const& signal) const
{
    if ((frameData == nullptr) || (signal.length == 0U) || (signal.length > SIGNAL_MAX_BIT_LENGTH))
    {
        return 0.0;
    }

    uint64_t const raw = extractRaw(frameData, signal.startBit, signal.length, signal.isMotorola);

    return rawToPhysical(raw, signal);
}

size_t SignalDatabase::getFrameCount() const { return m_frames.size(); }

size_t SignalDatabase::getSignalCount() const { return m_signalsByName.size(); }

bool SignalDatabase::parseDbcLine(std::string const& line, uint8_t const channelId)
{
    // Skip leading whitespace: DBC files indent SG_ lines.
    size_t const start = line.find_first_not_of(" \t\r");
    if (start == std::string::npos)
    {
        return false;
    }

    // Frame: BO_ <id> <name>: <dlc> <sender>
    if (line.compare(start, 4U, "BO_ ") == 0)
    {
        std::istringstream iss(line.substr(start + 4U));

        std::string idToken;
        iss >> idToken;
        if (idToken.empty())
        {
            logger::Logger::warn(logger::CANSTACK, "DBC: malformed BO_ line");
            return false;
        }

        // Remainder: "<name>: <dlc> <sender>"; the name is delimited by ':'.
        std::string const rest(std::istreambuf_iterator<char>(iss), {});
        size_t const nameStart = rest.find_first_not_of(' ');
        size_t const colonPos = rest.find(':');
        if ((colonPos == std::string::npos) || (nameStart == std::string::npos)
            || (nameStart > colonPos))
        {
            logger::Logger::warn(logger::CANSTACK, "DBC: malformed BO_ line");
            return false;
        }

        std::string const frameName = rest.substr(nameStart, colonPos - nameStart);

        int dlc = 0;
        std::istringstream dlcIss(rest.substr(colonPos + 1U));
        dlcIss >> dlc;
        if (dlc < 0)
        {
            logger::Logger::warn(logger::CANSTACK, "DBC: malformed BO_ dlc");
            return false;
        }

        uint32_t const rawId = static_cast<uint32_t>(strtoul(idToken.c_str(), nullptr, 10));
        uint32_t const frameId = rawId & 0x1FFFFFFFU;

        FrameConfig frame;
        frame.frameId   = frameId;
        frame.frameName = frameName;
        frame.dlc       = static_cast<uint8_t>(dlc);
        frame.channelId = channelId;

        storeFrame(frame);

        return true;
    }

    // Signal: SG_ <name> [M|m<N>] : <start>|<len>@<endian><sign> (f,o) [min|max] "unit" RX
    if (line.compare(start, 4U, "SG_ ") == 0)
    {
        std::istringstream iss(line.substr(start + 4U));

        std::string name;
        std::string muxOrColon;
        iss >> name >> muxOrColon;

        bool isSwitch = false;
        int16_t muxValue = -1;

        if (muxOrColon == "M")
        {
            isSwitch = true;
            iss >> muxOrColon;
        }
        else if ((muxOrColon.size() > 1U) && (muxOrColon[0] == 'm'))
        {
            muxValue = static_cast<int16_t>(atoi(muxOrColon.c_str() + 1));
            iss >> muxOrColon;
        }

        if (muxOrColon != ":")
        {
            logger::Logger::warn(logger::CANSTACK, "DBC: malformed SG_ line");
            return false;
        }

        // Remainder: <start>|<len>@<endian><sign> (factor,offset) [min|max] "unit" ...
        std::string rest(std::istreambuf_iterator<char>(iss), {});

        unsigned startBit = 0U;
        unsigned length = 0U;
        unsigned endianDigit = 0U;
        char sign = '+';

        int const parsed
            = std::sscanf(rest.c_str(), "%u|%u@%u%c", &startBit, &length, &endianDigit, &sign);
        if (parsed != 4)
        {
            logger::Logger::warn(logger::CANSTACK, "DBC: malformed SG_ bit layout");
            return false;
        }

        size_t parenPos = rest.find('(');
        size_t bracketPos = rest.find('[');
        size_t unitPos = rest.find('"');

        double factor = 1.0;
        double offset = 0.0;
        if (parenPos != std::string::npos)
        {
            (void)std::sscanf(rest.c_str() + parenPos, "(%lf,%lf", &factor, &offset);
        }

        double min = 0.0;
        double max = 0.0;
        if (bracketPos != std::string::npos)
        {
            (void)std::sscanf(rest.c_str() + bracketPos, "[%lf|%lf", &min, &max);
        }

        std::string unit;
        if (unitPos != std::string::npos)
        {
            size_t const unitEnd = rest.find('"', unitPos + 1U);
            if (unitEnd != std::string::npos)
            {
                unit = rest.substr(unitPos + 1U, unitEnd - unitPos - 1U);
            }
        }

        SignalConfig signal;
        signal.signalName = name;
        signal.frameId = 0U; // Bound to the most recent BO_ frame in storeSignal.
        signal.startBit = static_cast<uint8_t>(startBit);
        signal.length = static_cast<uint8_t>(length);
        signal.isMotorola = (endianDigit == 1U);
        signal.factor = (factor != 0.0) ? factor : 1.0;
        signal.offset = offset;
        signal.min = min;
        signal.max = max;
        signal.unit = unit;
        signal.channelId = channelId;
        signal.isMultiplexerSwitch = isSwitch;
        signal.multiplexValue = muxValue;
        signal.isSigned = (sign == '-');

        // Find the frame this signal belongs to: the last frame stored on this
        // channel is the message the SG_ line follows in a well formed DBC.
        if (!m_frames.empty())
        {
            // DBC files list SG_ lines right after their BO_ line.
            auto it = m_frames.rbegin();
            signal.frameId = it->second.frameId;
        }

        storeSignal(signal);

        return true;
    }

    return false;
}

void SignalDatabase::storeFrame(FrameConfig const& frame)
{
    auto const key = std::make_pair(frame.channelId, frame.frameId);

    auto it = m_frames.find(key);
    if (it == m_frames.end())
    {
        m_frames[key] = frame;
    }
    else
    {
        it->second.frameName = frame.frameName;
        it->second.dlc = frame.dlc;
    }
}

void SignalDatabase::storeSignal(SignalConfig const& signal)
{
    auto const key = std::make_pair(signal.channelId, signal.frameId);

    auto it = m_frames.find(key);
    if (it == m_frames.end())
    {
        // Implicit frame: derive the dlc from the signal layout.
        FrameConfig frame;
        frame.frameId = signal.frameId;
        frame.frameName = "";
        uint16_t const endBit = static_cast<uint16_t>(signal.startBit) + signal.length;
        frame.dlc = static_cast<uint8_t>((endBit + 7U) / 8U);
        frame.channelId = signal.channelId;
        m_frames[key] = frame;
        it = m_frames.find(key);
    }

    for (auto& existing : it->second.signals)
    {
        if (existing.signalName == signal.signalName)
        {
            existing = signal;
            m_signalsByName[signal.signalName] = signal;
            return;
        }
    }

    it->second.signals.push_back(signal);
    m_signalsByName[signal.signalName] = signal;
}

} // namespace canstack
