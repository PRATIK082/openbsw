/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "linlink/LinLinkManager.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace linlink
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

bool LinLinkManager::init(LinLinkManagerConfig const& config)
{
    if (config.maxNodes == 0U) return false;
    if (config.tTx == 0U) return false;
    if (config.userDataLength > 6U) return false;
    if (config.maxUsers == 0U) return false;

    cfg_ = config;
    if (cfg_.userDataLength > 6U) cfg_.userDataLength = 6U;

    lastSeen_.assign(cfg_.maxNodes, 0U);
    seen_.assign(cfg_.maxNodes, false);
    nodeUserData_.assign(cfg_.maxNodes, std::vector<uint8_t>(cfg_.userDataLength, 0U));
    nodePnGroup_.assign(cfg_.maxNodes, 0U);

    userRequests_.assign(cfg_.maxUsers, false);

    state_ = NmState::BusSleep;
    stateEntryTime_ = 0U;
    lastTxTime_ = 0U;
    lastNmPduRxTime_ = 0U;
    requestCount_ = 0U;
    commMode_ = CommMode::FullCommunication;
    for (uint8_t i = 0U; i < 8U; ++i) localUserData_[i] = 0U;

    initialised_ = true;
    return true;
}

void LinLinkManager::shutdown()
{
    initialised_ = false;
    state_ = NmState::BusSleep;
    requestCount_ = 0U;
    std::fill(userRequests_.begin(), userRequests_.end(), false);
}

// ---- User management ----
int8_t LinLinkManager::registerUser()
{
    if (!initialised_) return -1;
    for (uint8_t i = 0U; i < cfg_.maxUsers; ++i)
    {
        // find a slot not registered? We use userRequests_ as registration+request.
        // Simple: allow any index < maxUsers; track registration separately not needed.
        return static_cast<int8_t>(i);
    }
    return -1;
}

void LinLinkManager::userNetworkRequest(uint8_t userId)
{
    if (!initialised_) return;
    if (userId >= cfg_.maxUsers) return;
    if (commMode_ == CommMode::NoCommunication) return;
    if (!userRequests_[userId])
    {
        userRequests_[userId] = true;
        ++requestCount_;
        if (state_ == NmState::BusSleep || state_ == NmState::PrepareBusSleep ||
            state_ == NmState::ReadySleep)
        {
            if (wakeupCallback_) wakeupCallback_();
        }
    }
}

void LinLinkManager::userNetworkRelease(uint8_t userId)
{
    if (!initialised_) return;
    if (userId >= cfg_.maxUsers) return;
    if (userRequests_[userId])
    {
        userRequests_[userId] = false;
        if (requestCount_ > 0U) --requestCount_;
    }
}

void LinLinkManager::networkRequest()
{
    if (!initialised_) return;
    // aggregate: request on behalf of user 0
    userNetworkRequest(0U);
}

void LinLinkManager::networkRelease()
{
    if (!initialised_) return;
    userNetworkRelease(0U);
}

void LinLinkManager::setCommMode(CommMode mode)
{
    if (!initialised_) return;
    commMode_ = mode;
    if (mode == CommMode::NoCommunication)
    {
        // release all users
        for (uint8_t i = 0U; i < cfg_.maxUsers; ++i)
        {
            if (userRequests_[i])
            {
                userRequests_[i] = false;
                if (requestCount_ > 0U) --requestCount_;
            }
        }
    }
}

// ---- Receive path ----
bool LinLinkManager::isRelevantPdu(uint8_t pnGroup) const
{
    if (!cfg_.partialNetworkingEnabled) return true;
    // broadcast group 0 or matching group
    if (pnGroup == 0U) return true;
    return (pnGroup == cfg_.pnGroup);
}

void LinLinkManager::onNmPduReceived(uint8_t nodeId, uint8_t const* data, uint8_t dlc, uint32_t nowMs)
{
    if (!initialised_) return;
    if (nodeId >= cfg_.maxNodes) return;

    NmControlBits cb{};
    uint8_t src = 0U;
    uint8_t ud[8] = {0U};
    uint8_t udLen = 0U;
    uint8_t pnGroup = 0U;
    if (!decodePdu(data, dlc, cb, src, ud, udLen, &pnGroup)) return;

    if (!isRelevantPdu(pnGroup)) return;

    // track alive
    seen_[nodeId] = true;
    lastSeen_[nodeId] = nowMs;
    for (uint8_t i = 0U; i < cfg_.userDataLength && i < udLen; ++i)
        nodeUserData_[nodeId][i] = ud[i];
    nodePnGroup_[nodeId] = pnGroup;

    lastNmPduRxTime_ = nowMs;

    // remote wakeup (AW bit)
    if (cb.activeWakeup && (state_ == NmState::BusSleep || state_ == NmState::PrepareBusSleep))
    {
        if (wakeupCallback_) wakeupCallback_();
    }

    // passive wake-up
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
}

void LinLinkManager::onBusOff()
{
    if (!initialised_) return;
    if (busOffCallback_) busOffCallback_();
    // on bus-off, go to bus sleep to recover
    transitionTo(NmState::BusSleep, (lastNmPduRxTime_ != 0U) ? lastNmPduRxTime_ : 0U);
}

// ---- Periodic processing ----
void LinLinkManager::mainFunction(uint32_t nowMs)
{
    if (!initialised_) return;

    updateAliveNodes(nowMs);

    switch (state_)
    {
        case NmState::BusSleep:
            if (requestCount_ > 0U && commMode_ != CommMode::NoCommunication)
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
            if ((nowMs - stateEntryTime_) >= cfg_.tWaitBusSleep)
            {
                transitionTo(NmState::BusSleep, nowMs);
                if (sleepCallback_) sleepCallback_();
            }
            break;

        case NmState::RepeatMessage:
            transmitPdu(nowMs);
            if (requestCount_ == 0U && (nowMs - stateEntryTime_) >= cfg_.tRepeatMessage)
            {
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
            transmitPdu(nowMs);
            if (requestCount_ > 0U)
            {
                transitionTo(NmState::RepeatMessage, nowMs);
                break;
            }
            // NM coordinator: if coordinator and ready, set CSR already handled in TX
            if (lastNmPduRxTime_ == 0U || (nowMs - lastNmPduRxTime_) >= cfg_.tNm)
            {
                if (getAliveNodeCount() <= 1U)
                {
                    transitionTo(NmState::PrepareBusSleep, nowMs);
                }
            }
            break;
    }
}

void LinLinkManager::transitionTo(NmState next, uint32_t nowMs)
{
    if (next == state_) return;
    NmState old = state_;
    state_ = next;
    stateEntryTime_ = nowMs;
    lastTxTime_ = 0U;
    if (stateCallback_) stateCallback_(old, next);
    if (next == NmState::RepeatMessage || next == NmState::NormalOperation ||
        next == NmState::ReadySleep)
    {
        transmitPdu(nowMs);
    }
}

void LinLinkManager::transmitPdu(uint32_t nowMs)
{
    if (!cfg_.activeNm) return; // passive NM never transmits
    if (commMode_ == CommMode::SilentCommunication || commMode_ == CommMode::NoCommunication) return;
    if (txCallback_ == nullptr) return;
    if (lastTxTime_ != 0U && (nowMs - lastTxTime_) < cfg_.tTx) return;

    NmControlBits cb{};
    cb.repeatMessageRequest = (state_ == NmState::RepeatMessage);
    cb.nmCoordinatorSleepReady = (cfg_.nmCoordinator && state_ == NmState::ReadySleep);
    cb.activeWakeup = (requestCount_ > 0U);
    cb.partialNetworkInfo = cfg_.partialNetworkingEnabled;

    uint8_t buf[8] = {0U};
    // PN group carried in user data byte 0 when PN enabled
    uint8_t ud[8] = {0U};
    for (uint8_t i = 0U; i < cfg_.userDataLength; ++i) ud[i] = localUserData_[i];
    if (cfg_.partialNetworkingEnabled && cfg_.userDataLength > 0U)
    {
        ud[0] = cfg_.pnGroup;
    }

    uint8_t dlc = encodePdu(cfg_, cb, cfg_.nodeId, ud, buf, 8U);
    txCallback_(cfg_.nmPduId, dlc, buf);
    lastTxTime_ = nowMs;
}

void LinLinkManager::updateAliveNodes(uint32_t nowMs)
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

uint8_t LinLinkManager::getAliveNodeCount() const
{
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < cfg_.maxNodes; ++i)
        if (seen_[i]) ++n;
    if (cfg_.activeNm && state_ != NmState::BusSleep && state_ != NmState::PrepareBusSleep)
    {
        if (cfg_.nodeId < cfg_.maxNodes && !seen_[cfg_.nodeId])
            ++n;
    }
    return n;
}

bool LinLinkManager::isNodeAlive(uint8_t nodeId, uint32_t nowMs) const
{
    if (nodeId >= cfg_.maxNodes) return false;
    if (!seen_[nodeId]) return false;
    return (nowMs - lastSeen_[nodeId]) <= cfg_.tNm;
}

void LinLinkManager::getNodeUserData(uint8_t nodeId, uint8_t* out, uint8_t len) const
{
    if (out == nullptr || nodeId >= cfg_.maxNodes) return;
    if (!seen_[nodeId]) return;
    uint8_t n = std::min<uint8_t>(len, cfg_.userDataLength);
    for (uint8_t i = 0U; i < n; ++i) out[i] = nodeUserData_[nodeId][i];
}

void LinLinkManager::setUserData(uint8_t const* data, uint8_t len)
{
    uint8_t n = std::min<uint8_t>(len, 8U);
    for (uint8_t i = 0U; i < 8U; ++i) localUserData_[i] = 0U;
    if (data != nullptr)
        for (uint8_t i = 0U; i < n; ++i) localUserData_[i] = data[i];
}

void LinLinkManager::getUserData(uint8_t* out, uint8_t len) const
{
    if (out == nullptr) return;
    uint8_t n = std::min<uint8_t>(len, 8U);
    for (uint8_t i = 0U; i < n; ++i) out[i] = localUserData_[i];
}

uint8_t LinLinkManager::encodePdu(LinLinkManagerConfig const& cfg, NmControlBits const& cb,
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
    if (cb.partialNetworkInfo) cbv |= 0x08U;
    out[0] = cbv;
    out[1] = srcNodeId;

    for (uint8_t i = 0U; i < udLen && (2U + i) < dlc; ++i)
    {
        out[2U + i] = (userData != nullptr) ? userData[i] : 0U;
    }
    for (uint8_t i = dlc; i < outSize && i < 8U; ++i) out[i] = 0U;
    return dlc;
}

bool LinLinkManager::decodePdu(uint8_t const* data, uint8_t dlc,
                      NmControlBits& cb, uint8_t& srcNodeId,
                      uint8_t* userData, uint8_t& userDataLen,
                      uint8_t* pnGroup)
{
    if (data == nullptr || dlc < 2U) return false;
    uint8_t cbv = data[0];
    cb.repeatMessageRequest = (cbv & 0x01U) != 0U;
    cb.nmCoordinatorSleepReady = (cbv & 0x02U) != 0U;
    cb.activeWakeup = (cbv & 0x04U) != 0U;
    cb.partialNetworkInfo = (cbv & 0x08U) != 0U;
    srcNodeId = data[1];
    userDataLen = (dlc >= 2U) ? static_cast<uint8_t>(dlc - 2U) : 0U;
    if (userDataLen > 6U) userDataLen = 6U;
    if (userData != nullptr)
    {
        for (uint8_t i = 0U; i < userDataLen; ++i) userData[i] = data[2U + i];
    }
    if (pnGroup != nullptr && cb.partialNetworkInfo && userDataLen > 0U)
    {
        *pnGroup = data[2U];
    }
    return true;
}

} // namespace linlink
