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
 * \ingroup nm
 *
 * CAN Network Management (CanNm) — AUTOSAR-style NM state machine for CAN buses.
 * Provides node detection, alive tracking, sleep/wake coordination and NM PDU
 * handling. Tick-driven via mainFunction() (caller-provided tick, ms).
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

namespace nm
{

/// CanNm states (AUTOSAR NM state machine).
enum class NmState : uint8_t
{
    BusSleep,
    PrepareBusSleep,
    RepeatMessage,
    NormalOperation,
    ReadySleep
};

/// Parses a NmState name; false on unknown.
bool parseNmState(char const* text, NmState& state);

/// NM PDU control bit vector (byte 0).
struct NmControlBits
{
    bool repeatMessageRequest = false;   ///< RMR
    bool nmCoordinatorSleepReady = false; ///< CSR
    bool activeWakeup = false;           ///< AW
};

/// Configuration for one CanNm instance (one CAN channel / NM cluster).
struct CanNmConfig
{
    uint8_t nodeId = 0U;          ///< this node's NM node id
    uint32_t nmPduId = 0x500U;   ///< CAN frame id used for NM PDUs
    uint8_t userDataLength = 4U;  ///< user data bytes (0..6; clamped)

    // timings in ms (caller tick period must be consistent)
    uint32_t tNm = 2000U;            ///< NM timeout: node considered gone after this
    uint32_t tRepeatMessage = 1000U; ///< repeat-message phase duration
    uint32_t tWaitBusSleep = 1500U;  ///< wait-bus-sleep duration before sleeping
    uint32_t tTx = 500U;            ///< NM PDU transmit period

    bool activeNm = true;            ///< active (transmits) vs passive NM
    uint8_t maxNodes = 64U;          ///< max tracked nodes (capacity)
};

class CanNm
{
public:
    using TxCallback = std::function<bool(uint32_t frameId, uint8_t dlc, uint8_t const* data)>;
    using StateCallback = std::function<void(NmState oldState, NmState newState)>;
    using SleepCallback = std::function<void()>;   ///< notify upper layer (EcuMode) to sleep
    using WakeupCallback = std::function<void()>;  ///< notify upper layer of network activity

    CanNm() = default;
    ~CanNm() = default;

    CanNm(CanNm const&) = delete;
    CanNm& operator=(CanNm const&) = delete;

    /// Initialises the instance. Returns false on invalid config.
    bool init(CanNmConfig const& config);

    /// Releases resources / resets state.
    void shutdown();

    /// Upper-layer network request (increment user count).
    void networkRequest();

    /// Upper-layer network release (decrement user count; clamped at 0).
    void networkRelease();

    /// Receive path: inject a received NM PDU. nodeId is the sender's NM node id.
    void onNmPduReceived(uint8_t nodeId, uint8_t const* data, uint8_t dlc, uint32_t nowMs);

    /// Periodic processing. nowMs is the caller-provided tick (ms).
    void mainFunction(uint32_t nowMs);

    /// State query.
    NmState getState() const { return state_; }

    /// True when at least one user holds a network request.
    bool isNetworkRequested() const { return requestCount_ > 0U; }

    /// Number of currently alive nodes (including self if active).
    uint8_t getAliveNodeCount() const;

    /// True if the given node is currently alive.
    bool isNodeAlive(uint8_t nodeId, uint32_t nowMs) const;

    /// Last received user data for a node (0 if unknown).
    void getNodeUserData(uint8_t nodeId, uint8_t* out, uint8_t len) const;

    /// Local user data.
    void setUserData(uint8_t const* data, uint8_t len);
    void getUserData(uint8_t* out, uint8_t len) const;

    /// Callbacks.
    void setTxCallback(TxCallback cb) { txCallback_ = std::move(cb); }
    void setStateCallback(StateCallback cb) { stateCallback_ = std::move(cb); }
    void setSleepCallback(SleepCallback cb) { sleepCallback_ = std::move(cb); }
    void setWakeupCallback(WakeupCallback cb) { wakeupCallback_ = std::move(cb); }

    /// Encode an NM PDU into buffer (size >= 2+userDataLength). Returns DLC.
    static uint8_t encodePdu(CanNmConfig const& cfg, NmControlBits const& cb,
                             uint8_t srcNodeId, uint8_t const* userData,
                             uint8_t* out, uint8_t outSize);

    /// Decode an NM PDU. Returns false on invalid size.
    static bool decodePdu(uint8_t const* data, uint8_t dlc,
                          NmControlBits& cb, uint8_t& srcNodeId,
                          uint8_t* userData, uint8_t& userDataLen);

private:
    void transitionTo(NmState next, uint32_t nowMs);
    void transmitPdu(uint32_t nowMs);
    void updateAliveNodes(uint32_t nowMs);
    bool isBusSleepAllowed(uint32_t nowMs) const;

    CanNmConfig cfg_{};
    bool initialised_ = false;

    NmState state_ = NmState::BusSleep;
    uint32_t stateEntryTime_ = 0U;
    uint32_t lastTxTime_ = 0U;
    uint32_t lastNmPduRxTime_ = 0U;

    uint32_t requestCount_ = 0U;

    // alive tracking: last-seen time per node id; 0 means never seen (use a separate flag)
    std::vector<uint32_t> lastSeen_;
    std::vector<bool> seen_;
    std::vector<std::vector<uint8_t>> nodeUserData_;

    uint8_t localUserData_[8] = {0U};

    TxCallback txCallback_;
    StateCallback stateCallback_;
    SleepCallback sleepCallback_;
    WakeupCallback wakeupCallback_;
};

} // namespace nm
