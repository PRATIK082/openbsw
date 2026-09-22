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
 * \ingroup commgateway
 */
#include "commgateway/LinChannel.h"

namespace commgateway
{

void LinChannel::init(LinHwInterface* hw, uint8_t channelId)
{
    m_hw          = hw;
    m_channelId   = channelId;
    m_initialized = (hw != nullptr);
    if (m_initialized)
    {
        m_hw->registerRxCallback(0U, [this](uint8_t pid, uint8_t* data, uint8_t dlc) {
            onFrameReceived(pid, data, dlc);
        });
        m_hw->registerTxDoneCallback(
            [this](uint8_t pid, bool success) { onFrameTransmitted(pid, success); });
    }
}

void LinChannel::shutdown()
{
    m_initialized = false;
    m_hw          = nullptr;
    m_scheduleTable.clear();
    m_scheduler.clear();
}

void LinChannel::scheduleFrame(LinFrameConfig const& config)
{
    for (auto& existing : m_scheduleTable)
    {
        if (existing.pid == config.pid)
        {
            existing = config;
            return;
        }
    }
    m_scheduleTable.push_back(config);
    if (config.scheduleTimeMs > 0U)
    {
        LinScheduleEntry entry{};
        entry.pid          = config.pid;
        entry.dlc          = config.dlc;
        entry.cycleTimeMs  = config.scheduleTimeMs;
        entry.enabled      = true;
        m_scheduler.addEntry(entry);
    }
}

bool LinChannel::transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc)
{
    if (!m_initialized || (m_hw == nullptr) || m_sleeping)
    {
        return false;
    }
    return m_hw->transmitFrame(pid, data, dlc);
}

void LinChannel::onFrameReceived(uint8_t pid, uint8_t* data, uint8_t dlc)
{
    if (!isPidValid(pid))
    {
        reportError(LinError::PARITY);
        return;
    }
    if (m_upperCallback)
    {
        m_upperCallback(m_channelId, pid, data, dlc);
    }
}

void LinChannel::onFrameTransmitted(uint8_t /* pid */, bool success)
{
    if (!success)
    {
        reportError(LinError::TIMEOUT);
    }
}

void LinChannel::registerUpperLayerCallback(LinUpperCallback cb) { m_upperCallback = cb; }

void LinChannel::reportError(LinError error)
{
    m_errorCounts[static_cast<uint8_t>(error)]++;
}

void LinChannel::mainFunction(uint32_t nowMs)
{
    if (!m_initialized || (m_hw == nullptr))
    {
        return;
    }
    m_scheduler.tick(nowMs, [this](uint8_t pid, uint8_t dlc) {
        (void)dlc;
        LinFrameConfig const* frame = findFrame(pid);
        uint8_t data[8U]            = {0U};
        uint8_t len                 = (frame != nullptr) ? frame->dlc : 0U;
        return transmitFrame(pid, data, len);
    });
    m_hw->mainFunction();
}

void LinChannel::enterSleepMode()
{
    m_sleeping = true;
    if (m_hw != nullptr)
    {
        m_hw->enterSleepMode();
    }
}

void LinChannel::wakeup()
{
    m_sleeping = false;
    if (m_hw != nullptr)
    {
        m_hw->wakeup();
    }
}

bool LinChannel::isSleeping() const { return m_sleeping; }

uint8_t LinChannel::computePid(uint8_t id)
{
    uint8_t const id6 = id & 0x3FU;
    uint8_t const p0  = ((id6 >> 0U) ^ (id6 >> 1U) ^ (id6 >> 2U) ^ (id6 >> 4U)) & 1U;
    uint8_t const p1  = (~((id6 >> 1U) ^ (id6 >> 3U) ^ (id6 >> 4U) ^ (id6 >> 5U))) & 1U;
    return static_cast<uint8_t>(id6 | (p0 << 6U) | (p1 << 7U));
}

bool LinChannel::isPidValid(uint8_t pid) { return computePid(pid & 0x3FU) == pid; }

uint8_t LinChannel::computeChecksum(uint8_t pid, uint8_t const* data, uint8_t dlc, bool enhanced)
{
    uint16_t sum = 0U;
    if (enhanced)
    {
        sum += pid;
    }
    for (uint8_t i = 0U; i < dlc; ++i)
    {
        sum += data[i];
        if (sum > 0xFFU)
        {
            sum = (sum & 0xFFU) + 1U;
        }
    }
    return static_cast<uint8_t>(~sum & 0xFFU);
}

LinFrameConfig const* LinChannel::findFrame(uint8_t pid) const
{
    for (auto const& frame : m_scheduleTable)
    {
        if (frame.pid == pid)
        {
            return &frame;
        }
    }
    return nullptr;
}

uint8_t LinChannel::getChannelId() const { return m_channelId; }

bool LinChannel::isInitialized() const { return m_initialized; }

uint32_t LinChannel::getErrorCount(LinError error) const
{
    return m_errorCounts[static_cast<uint8_t>(error)];
}

} // namespace commgateway
