/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "nm/CanNm.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace nm
{

bool parseNmState(char const* text, NmState& state)
{
    if (text == nullptr) return false;
    std::string s(text);
    if (s == "BusSleep") { state = NmState::BusSleep; return true; }
    if (s == "PrepareBusSleep") { state = NmState::PrepareBusSleep; return true; }
    if (s == "RepeatMessage") { state = NmState::RepeatMessage; return true; }
    if (s == "NormalOperation") { state = NmState::NormalOperation; return true; }
    if (s == "ReadySleep") { state = NmState::ReadySleep; return true; }
    return false;
}

bool CanNm::init(CanNmConfig const& config)
{
    if (config.maxNodes == 0U) return false;
    if (config.tTx == 0U) return false;
    if (config.userDataLength > 6U) return false;

    cfg_ = config;
    if (cfg_.userDataLength > 6U) cfg_.userDataLength = 6U;

    lastSeen_.assign(cfg_.maxNodes, 0U);
    seen_.assign(cfg_.maxNodes, false);
    nodeUserData_.assign(cfg_.maxNodes, std::vector<uint8_t>(cfg_.userDataLength, 0U));

    state_ = NmState::BusSleep;
    stateEntryTime_ = 0U;
    lastTxTime_ = 0U;
    lastNmPduRxTime_ = 0U;
    requestCount_ = 0U;
    for (uint8_t i = 0U; i < 8U; ++i) localUserData_[i] = 0U;

    initialised_ = true;
    return true;
}

void CanNm::shutdown()
{
    initialised_ = false;
    state_ = NmState::BusSleep;
    requestCount_ = 0U;
}

void CanNm::networkRequest()
{
    if (!initialised_) return;
    ++requestCount_;
    if (state_ == NmState::BusSleep || state_ == NmState::PrepareBusSleep ||
        state_ == NmState::ReadySleep)
    {
        // wake up
        if (wakeupCallback_) wakeupCallback_();
    }
}

void CanNm::networkRelease()
{
    if (!initialised_) return;
    if (requestCount_ > 0U) --requestCount_;
}

void CanNm::onNmPduReceived(uint8_t nodeId, uint8_t const* data, uint8_t dlc, uint32_t nowMs)
{
    if (!initialised_) return;
    if (nodeId >= cfg_.maxNodes) return;

    // decode
    NmControlBits cb{};
    uint8_t src = 0U;
    uint8_t ud[8] = {0U};
    uint8_t udLen = 0U;
    if (!decodePdu(data, dlc, cb, src, ud, udLen)) return;

    // track alive
    seen_[nodeId] = true;
    lastSeen_[nodeId] = nowMs;
    for (uint8_t i = 0U; i < cfg_.userDataLength && i < udLen; ++i)
        nodeUserData_[nodeId][i] = ud[i];

    lastNmPduRxTime_ = nowMs;

    // passive wake-up: receiving an NM PDU while sleeping wakes the network
    if (state_ == NmState::BusSleep)
    {
        if (wakeupCallback_) wakeupCallback_();
        transitionTo(NmState::RepeatMessage, nowMs);
        return;
    }
    if (state_ == NmState::PrepareBusSleep)
    {
        transitionTo(NmState::RepeatMessage, nowMs);
        return;
    }
    if (state_ == NmState::ReadySleep)
    {
        transitionTo(NmState::RepeatMessage, nowMs);
        return;
    }
    // in RepeatMessage/NormalOperation just refresh
}

void CanNm::mainFunction(uint32_t nowMs)
{
    if (!initialised_) return;

    updateAliveNodes(nowMs);

    switch (state_)
    {
        case NmState::BusSleep:
            // nothing; waiting for request or RX (handled in onNmPduReceived/networkRequest)
            // But networkRequest() only sets counter; need to transition here:
            if (requestCount_ > 0U)
            {
                if (wakeupCallback_) wakeupCallback_();
                transitionTo(NmState::RepeatMessage, nowMs);
            }
            break;

        case NmState::PrepareBusSleep:
            if (requestCount_ > 0U)
            {
                transitionTo(NmState::RepeatMessage, nowMs);
                break;
            }
            // if an NM PDU arrives, handled in onNmPduReceived -> RepeatMessage
            if ((nowMs - stateEntryTime_) >= cfg_.tWaitBusSleep)
            {
                // sleep
                transitionTo(NmState::BusSleep, nowMs);
                if (sleepCallback_) sleepCallback_();
            }
            break;

        case NmState::RepeatMessage:
            transmitPdu(nowMs);
            if (requestCount_ == 0U && (nowMs - stateEntryTime_) >= cfg_.tRepeatMessage)
            {
                // no requests -> ready sleep
                transitionTo(NmState::ReadySleep, nowMs);
            }
            else if (requestCount_ > 0U && (nowMs - stateEntryTime_) >= cfg_.tRepeatMessage)
            {
                transitionTo(NmState::NormalOperation, nowMs);
            }
            break;

        case NmState::NormalOperation:
            transmitPdu(nowMs);
            if (requestCount_ == 0U)
            {
                transitionTo(NmState::ReadySleep, nowMs);
            }
            break;

        case NmState::ReadySleep:
            // listen; transmit occasionally? Ready sleep: node stays awake but indicates ready to sleep
            transmitPdu(nowMs);
            if (requestCount_ > 0U)
            {
                transitionTo(NmState::RepeatMessage, nowMs);
                break;
            }
            // if no NM PDUs received for tNm -> prepare bus sleep
            if (lastNmPduRxTime_ == 0U || (nowMs - lastNmPduRxTime_) >= cfg_.tNm)
            {
                // but only if no other nodes alive
                if (getAliveNodeCount() <= 1U) // only self
                {
                    transitionTo(NmState::PrepareBusSleep, nowMs);
                }
            }
            break;
    }
}

void CanNm::transitionTo(NmState next, uint32_t nowMs)
{
    if (next == state_) return;
    NmState old = state_;
    state_ = next;
    stateEntryTime_ = nowMs;
    lastTxTime_ = 0U; // force tx on next
    if (stateCallback_) stateCallback_(old, next);
    // Transmit immediately on entering a communicating state.
    if (next == NmState::RepeatMessage || next == NmState::NormalOperation ||
        next == NmState::ReadySleep)
    {
        transmitPdu(nowMs);
    }
}

void CanNm::transmitPdu(uint32_t nowMs)
{
    if (!cfg_.activeNm) return;
    if (txCallback_ == nullptr) return;
    if (lastTxTime_ != 0U && (nowMs - lastTxTime_) < cfg_.tTx) return;

    NmControlBits cb{};
    cb.repeatMessageRequest = (state_ == NmState::RepeatMessage);
    cb.nmCoordinatorSleepReady = (state_ == NmState::ReadySleep);
    cb.activeWakeup = (requestCount_ > 0U);

    uint8_t buf[8] = {0U};
    uint8_t dlc = encodePdu(cfg_, cb, cfg_.nodeId, localUserData_, buf, 8U);
    txCallback_(cfg_.nmPduId, dlc, buf);
    lastTxTime_ = nowMs;
}

void CanNm::updateAliveNodes(uint32_t nowMs)
{
    for (uint8_t i = 0U; i < cfg_.maxNodes; ++i)
    {
        if (!seen_[i]) continue;
        if ((nowMs - lastSeen_[i]) > cfg_.tNm)
        {
            seen_[i] = false;
            lastSeen_[i] = 0U;
        }
    }
}

uint8_t CanNm::getAliveNodeCount() const
{
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < cfg_.maxNodes; ++i)
        if (seen_[i]) ++n;
    // include self if active and in a non-sleep state
    if (cfg_.activeNm && state_ != NmState::BusSleep && state_ != NmState::PrepareBusSleep)
    {
        // count self if not already counted
        if (cfg_.nodeId < cfg_.maxNodes && !seen_[cfg_.nodeId])
            ++n;
    }
    return n;
}

bool CanNm::isNodeAlive(uint8_t nodeId, uint32_t nowMs) const
{
    if (nodeId >= cfg_.maxNodes) return false;
    if (!seen_[nodeId]) return false;
    return (nowMs - lastSeen_[nodeId]) <= cfg_.tNm;
}

void CanNm::getNodeUserData(uint8_t nodeId, uint8_t* out, uint8_t len) const
{
    if (out == nullptr || nodeId >= cfg_.maxNodes) return;
    if (!seen_[nodeId]) return;
    uint8_t n = std::min<uint8_t>(len, cfg_.userDataLength);
    for (uint8_t i = 0U; i < n; ++i) out[i] = nodeUserData_[nodeId][i];
}

void CanNm::setUserData(uint8_t const* data, uint8_t len)
{
    uint8_t n = std::min<uint8_t>(len, 8U);
    for (uint8_t i = 0U; i < 8U; ++i) localUserData_[i] = 0U;
    if (data != nullptr)
        for (uint8_t i = 0U; i < n; ++i) localUserData_[i] = data[i];
}

void CanNm::getUserData(uint8_t* out, uint8_t len) const
{
    if (out == nullptr) return;
    uint8_t n = std::min<uint8_t>(len, 8U);
    for (uint8_t i = 0U; i < n; ++i) out[i] = localUserData_[i];
}

uint8_t CanNm::encodePdu(CanNmConfig const& cfg, NmControlBits const& cb,
                         uint8_t srcNodeId, uint8_t const* userData,
                         uint8_t* out, uint8_t outSize)
{
    if (out == nullptr || outSize < 2U) return 0U;
    uint8_t udLen = cfg.userDataLength;
    if (udLen > 6U) udLen = 6U;
    uint8_t dlc = static_cast<uint8_t>(2U + udLen);
    if (dlc > outSize) dlc = outSize;
    if (dlc < 2U) dlc = 2U;

    uint8_t cbv = 0U;
    if (cb.repeatMessageRequest) cbv |= 0x01U;
    if (cb.nmCoordinatorSleepReady) cbv |= 0x02U;
    if (cb.activeWakeup) cbv |= 0x04U;
    out[0] = cbv;
    out[1] = srcNodeId;

    for (uint8_t i = 0U; i < udLen && (2U + i) < dlc; ++i)
    {
        out[2U + i] = (userData != nullptr) ? userData[i] : 0U;
    }
    // zero fill
    for (uint8_t i = dlc; i < outSize && i < 8U; ++i) out[i] = 0U;
    return dlc;
}

bool CanNm::decodePdu(uint8_t const* data, uint8_t dlc,
                      NmControlBits& cb, uint8_t& srcNodeId,
                      uint8_t* userData, uint8_t& userDataLen)
{
    if (data == nullptr || dlc < 2U) return false;
    uint8_t cbv = data[0];
    cb.repeatMessageRequest = (cbv & 0x01U) != 0U;
    cb.nmCoordinatorSleepReady = (cbv & 0x02U) != 0U;
    cb.activeWakeup = (cbv & 0x04U) != 0U;
    srcNodeId = data[1];
    userDataLen = (dlc >= 2U) ? static_cast<uint8_t>(dlc - 2U) : 0U;
    if (userDataLen > 6U) userDataLen = 6U;
    if (userData != nullptr)
    {
        for (uint8_t i = 0U; i < userDataLen; ++i) userData[i] = data[2U + i];
    }
    return true;
}

} // namespace nm
