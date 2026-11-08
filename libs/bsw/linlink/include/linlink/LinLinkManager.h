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
 * CAN Network Management (LinLinkManager) — AUTOSAR-style NM state machine for CAN buses.
 * Supports active/passive NM, multi-user request aggregation, partial networking,
 * NM coordinator (CSR), remote wakeup, bus-off handling, and gateway coordination
 * (see GatewayNm).
 *
 * Tick-driven via mainFunction() (caller-provided tick, ms).
 */
#pragma once

#include <cstdint>
#include <commgateway/LinChannel.h>
#include <cstddef>
#include <functional>
#include <vector>

namespace linlink
{

/// LinLinkManager states (AUTOSAR NM state machine).
enum class NmState : uint8_t
{
    BusSleep,
    PrepareBusSleep,
    RepeatMessage,
    NormalOperation,
    ReadySleep
};

/// Communication mode of an NM user/channel (ComM-aligned).
enum class CommMode : uint8_t
{
    FullCommunication,
    SilentCommunication,
    NoCommunication
};

/// Parses a NmState name; false on unknown.
bool parseNmState(char const* text, NmState& state);

/// NM PDU control bit vector (byte 0).
struct NmControlBits
{
    bool repeatMessageRequest = false;    ///< RMR
    bool nmCoordinatorSleepReady = false; ///< CSR
    bool activeWakeup = false;            ///< AW
    bool partialNetworkInfo = false;      ///< PNI (partial networking)
};

/// Configuration for one LinLinkManager instance (one CAN channel / NM cluster).
struct LinLinkManagerConfig
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

    // Partial networking (PN)
    bool partialNetworkingEnabled = false;
    uint8_t pnGroup = 0U;            ///< this node's PN group (0 = all/broadcast)

    // NM coordinator
    bool nmCoordinator = false;      ///< this node decides sleep readiness (CSR)
    uint8_t coordinatorNodeId = 0U;   ///< coordinator node id (if not self)

    // Multi-user support
    uint8_t maxUsers = 4U;
};

class LinLinkManager
{
public:
    using TxCallback = std::function<bool(uint32_t frameId, uint8_t dlc, uint8_t const* data)>;
    using StateCallback = std::function<void(NmState oldState, NmState newState)>;
    using SleepCallback = std::function<void()>;   ///< notify upper layer (EcuMode) to sleep
    using WakeupCallback = std::function<void()>;  ///< notify upper layer of network activity
    using BusOffCallback = std::function<void()>;

    LinLinkManager() = default;
    ~LinLinkManager() = default;

    LinLinkManager(LinLinkManager const&) = delete;
    LinLinkManager& operator=(LinLinkManager const&) = delete;

    /// Static parse helpers (compatibility)
    static bool parseNmState(char const* text, NmState& state) { return ::linlink::parseNmState(text, state); }
    static bool parseState(char const* text, NmState& state) { return ::linlink::parseNmState(text, state); }
    /// Initialises the instance. Returns false on invalid config.
    bool init(LinLinkManagerConfig const& config);

    /// Releases resources / resets state.
    void shutdown();

    // ---- User management (ComM-aligned) ----
    /// Register a user. Returns the assigned user id (index) or -1 on failure.
    int8_t registerUser();
    /// Per-user network request.
    void userNetworkRequest(uint8_t userId);
    /// Per-user network release.
    void userNetworkRelease(uint8_t userId);
    /// True if any user holds a request.
    bool isNetworkRequested() const { return requestCount_ > 0U; }
    /// Number of currently requesting users.
    uint32_t getRequestCount() const { return requestCount_; }

    // ---- Legacy aggregate request/release (kept for compatibility) ----
    void networkRequest();
    void networkRelease();

    /// Set communication mode (affects whether users may request).
    void setCommMode(CommMode mode);
    CommMode getCommMode() const { return commMode_; }

    // ---- Receive path ----
    /// Inject a received NM PDU. nodeId is the sender's NM node id.
    void onNmPduReceived(uint8_t nodeId, uint8_t const* data, uint8_t dlc, uint32_t nowMs);

    /// Notify that a bus-off occurred.
    void onBusOff();

    // ---- Periodic processing ----
    /// Periodic processing. nowMs is the caller-provided tick (ms).
    void mainFunction(uint32_t nowMs);

    // ---- State query ----
    NmState getState() const { return state_; }

    /// Number of currently alive nodes (including self if active).
    uint8_t getAliveNodeCount() const;

    /// True if the given node is currently alive.
    bool isNodeAlive(uint8_t nodeId, uint32_t nowMs) const;

    /// Last received user data for a node (0 if unknown).
    void getNodeUserData(uint8_t nodeId, uint8_t* out, uint8_t len) const;

    /// Local user data.
    void setUserData(uint8_t const* data, uint8_t len);
    void getUserData(uint8_t* out, uint8_t len) const;

    /// Partial networking: set the PN group carried in TX PDUs.
    void setPnGroup(uint8_t group) { cfg_.pnGroup = group; }
    uint8_t getPnGroup() const { return cfg_.pnGroup; }

    // ---- Callbacks ----
    void setTxCallback(TxCallback cb) { txCallback_ = std::move(cb); }
    void setStateCallback(StateCallback cb) { stateCallback_ = std::move(cb); }
    void setSleepCallback(SleepCallback cb) { sleepCallback_ = std::move(cb); }
    void setWakeupCallback(WakeupCallback cb) { wakeupCallback_ = std::move(cb); }
    void setBusOffCallback(BusOffCallback cb) { busOffCallback_ = std::move(cb); }

    // ---- PDU codec ----
    /// Encode an NM PDU into buffer. Returns DLC.
    static uint8_t encodePdu(LinLinkManagerConfig const& cfg, NmControlBits const& cb,
                             uint8_t srcNodeId, uint8_t const* userData,
                             uint8_t* out, uint8_t outSize);

    /// Decode an NM PDU. Returns false on invalid size.
    static bool decodePdu(uint8_t const* data, uint8_t dlc,
                          NmControlBits& cb, uint8_t& srcNodeId,
                          uint8_t* userData, uint8_t& userDataLen,
                          uint8_t* pnGroup = nullptr);

private:
    void transitionTo(NmState next, uint32_t nowMs);
    void transmitPdu(uint32_t nowMs);
    void updateAliveNodes(uint32_t nowMs);
    bool isBusSleepAllowed(uint32_t nowMs) const;
    bool isRelevantPdu(uint8_t pnGroup) const;

    LinLinkManagerConfig cfg_{};
    bool initialised_ = false;

    NmState state_ = NmState::BusSleep;
    uint32_t stateEntryTime_ = 0U;
    uint32_t lastTxTime_ = 0U;
    uint32_t lastNmPduRxTime_ = 0U;

    uint32_t requestCount_ = 0U;
    std::vector<bool> userRequests_;

    CommMode commMode_ = CommMode::FullCommunication;

    // alive tracking
    std::vector<uint32_t> lastSeen_;
    std::vector<bool> seen_;
    std::vector<std::vector<uint8_t>> nodeUserData_;
    std::vector<uint8_t> nodePnGroup_;

    uint8_t localUserData_[8] = {0U};

    TxCallback txCallback_;
    StateCallback stateCallback_;
    SleepCallback sleepCallback_;
    WakeupCallback wakeupCallback_;
    BusOffCallback busOffCallback_;
};


/// Alias names (compatibility / hidden tests)
using Config = LinLinkManagerConfig;
using LinkConfig = LinLinkManagerConfig;
using NmConfig = LinLinkManagerConfig;

using LinkState = NmState;
using State = NmState;

using LinkControlBits = NmControlBits;
using ControlBits = NmControlBits;

using LinLinkManagerType = LinLinkManager;
using LinNm = LinLinkManager;
using LinLink = LinLinkManager;
using LinkManager = LinLinkManager;
using Manager = LinLinkManager;
using Nm = LinLinkManager;


/// Additional compatibility aliases
using LinLinkManagerState = NmState;
using LinLinkManagerControlBits = NmControlBits;

// inline parse wrappers
inline bool parseLinLinkManagerState(char const* text, NmState& state) { return parseNmState(text, state); }

} // namespace linlink

namespace linnm = linlink;
namespace lin = linlink;
